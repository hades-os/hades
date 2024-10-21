
#include <cstddef>
#include <util/ring.hpp>
#include <mm/mm.hpp>

template <typename T>
util::ring<T>::ring(size_t size) {
    this->data = (T *) kmalloc(sizeof(T) * size);
    this->size = size;
    this->head = -1;
    this->tail = -1;
}

template<typename T>
void util::ring<T>::push(T obj) {
    if ((head == 0 && tail == (size - 1)) || (head == (tail - 1))) {
        return;
    }

    if (head == -1) {
        head = 0;
        tail = 0;
    } else {
        if (tail == (size - 1)) {
            tail = 0;
        } else {
            tail++;
        }
    }

    data[tail] = obj;
    __atomic_add_fetch(&items, 1, __ATOMIC_RELAXED);
}

template <typename T>
T util::ring<T>::pop() {
    if (head == -1) {
        return;
    }

    T res = data[head];
    __atomic_sub_fetch(&items, 1, __ATOMIC_RELAXED);
    if (head == tail) {
        head = -1;
        tail = -1;
    } else {
        if (head == (size - 1)) {
            head = 0;
        } else {
            head++;
        }
    }

    return res;
} 

template <typename T>
T util::ring<T>::pop_back() {
    if (head == tail) {
        return;
    }

    if (tail == 0) {
        tail = size - 1;
    } else {
        tail--;
    }

    T out = data[tail];
    __atomic_sub_fetch(&items, 1, __ATOMIC_RELAXED);

    return out;
}

template <typename T>
const T util::ring<T>::peek() {
    if (head == 0) {
        return;
    }

    return data[head];
}