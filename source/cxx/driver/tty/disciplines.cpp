#include "mm/mm.hpp"
#include "sys/irq.hpp"
#include "sys/smp.hpp"
#include "util/ring.hpp"
#include <cstddef>
#include <driver/tty/tty.hpp>
#include <driver/tty/termios.hpp>

bool ignore_char(tty::termios termios, char c) {
    if (c == '\r' && (termios.c_iflag & IGNCR)) {
        return true;
    }

    return false;
}

char correct_char(tty::termios termios, char c) {
    if (c == '\r' && (termios.c_iflag & ICRNL)) {
        return termios.c_cc[VEOL];
    }

    if (c == termios.c_cc[VEOL] && (termios.c_iflag & INLCR)) {
        return '\r';
    }

    return c;
}

void tty::echo_char(device *tty, char c) {
    if (!(tty->termios.c_lflag & ECHO)) {
        return;
    }

    if (c > '\0' && c < ' ' && c != tty->termios.c_cc[VEOL] && c != '\t') {
        if (!(tty->termios.c_lflag & ECHOCTL)) {
            return;
        }

        char special[] = { '^',  (char) (c + 64) };
        tty->out.push(special[0]);
        tty->out.push(special[1]);

        return;
    }

    tty->out.push(c);
}

bool new_line(tty::termios termios, char c) {
	if(c == termios.c_cc[VEOL] || c == '\t' || c >= 32) {
		return true;
	}

	for(size_t i = 0; i < NCCS; i++) {
		if(termios.c_cc[i] == c) {
			return false;
		}
	}

	return true;    
}

ssize_t tty::device::read_canon(void *buf, size_t len) {
    char *chars = (char *) buf;
    size_t count = 0;
    canon_lock.irq_acquire();
    acquire_chars:
        util::ring<char> *line_queue;

        if ((line_queue = canon.peek())) {
            for (count = 0; count < len; count++) {
                if (!line_queue->pop(chars)) {
                    break;
                }

                chars++;
            }

            if (line_queue->items == 0) {
                canon.pop(&line_queue);
                frg::destruct(memory::mm::heap, line_queue);
            }

            canon_lock.irq_release();
            return count;
        }

        char c, special;
        size_t items = 0;
        line_queue = frg::construct<util::ring<char>>(memory::mm::heap, max_canon_lines);
        canon.push(line_queue);

        while (true) {
            irq::on();

            while (__atomic_load_n(&in.items, __ATOMIC_RELAXED) == 0) {
                if (smp::get_process() && smp::get_process()->sig_queue.sigpending) {
                    canon_lock.irq_release();
                    // TODO: errno
                    return -1;
                }
            }

            in_lock.irq_acquire();
            while (in.pop(&c)) {
                if (ignore_char(termios, c)) {
                    continue;
                }

                c = correct_char(termios, c);
                if (new_line(termios, c)) {
                    line_queue->push(c);
                    items++;
                    out_lock.irq_acquire();
                    echo_char(this, c);
                    out_lock.irq_release();

                    if (driver && driver->has_flush)
                        driver->flush(this);
                }

                if (c == termios.c_cc[VEOL] || c == termios.c_cc[VEOF]) {
                    in_lock.irq_release();
                    goto acquire_chars;
                }
                
                if ((termios.c_lflag & ECHOE) && (c == termios.c_cc[VERASE])) {
                    // Print a backspace and ignore the char
                    if (items) {
                        items--;
                        char special2[] = { '\b', ' ', '\b' };

                        out_lock.irq_acquire();
                        out.push(special2[0]);
                        out.push(special2[1]);
                        out.push(special2[2]);
                        out_lock.irq_release();

                        if (driver && driver->has_flush)
                            driver->flush(this);
                        line_queue->pop_back(&special);
                    }
                }
            }

            in_lock.irq_release();
        }

    goto acquire_chars;
}

ssize_t tty::device::read_raw(void *buf, size_t len) {
    cc_t min = termios.c_cc[VMIN];
    cc_t time = termios.c_cc[VTIME];
    char *chars = (char *) buf;
    size_t count = 0;

    if (min == 0 && time == 0) {
        if (__atomic_load_n(&in.items, __ATOMIC_RELAXED) == 0) {
            return 0;
        }

        in_lock.irq_acquire();
        out_lock.irq_acquire();
        for (count = 0; count < len; count++) {
            if (!in.pop(chars)) {
                break;
            }

            if (ignore_char(termios, *chars)) {
                continue;
            }

            *chars = correct_char(termios, *chars);
            echo_char(this, *chars++);
        }

        out_lock.irq_release();
        if (driver && driver->has_flush)
            driver->flush(this);
        in_lock.irq_release();

        return count;
    } else if (min > 0 && time == 0) {
        irq::on();

        while (__atomic_load_n(&in.items, __ATOMIC_RELAXED) < min) {
            if (smp::get_process() && smp::get_process()->sig_queue.sigpending) {
                // TODO: errno
                return -1;
            }
        }

        in_lock.irq_acquire();
        out_lock.irq_acquire();
        for (count = 0; count < len; count++) {
            in.pop(chars);
            if (ignore_char(termios, *chars)) {
                continue;
            }

            *chars = correct_char(termios, *chars);
            echo_char(this, *chars++);
        }

        out_lock.irq_release();
        if (driver && driver->has_flush)
            driver->flush(this);
        in_lock.irq_release();

        return count;
    } else {
        // TODO: time != but min < 0
        return -1;
    }
}