#ifndef DEVFS_HPP
#define DEVFS_HPP

#include "fs/poll.hpp"
#include "mm/arena.hpp"
#include "mm/common.hpp"
#include "util/types.hpp"
#include <cstddef>
#include <frg/hash.hpp>
#include <frg/hash_map.hpp>
#include <frg/tuple.hpp>
#include <fs/vfs.hpp>
#include <mm/mm.hpp>
#include <util/errors.hpp>

namespace cache {
    class holder;
}

namespace vfs {
    class devfs : public vfs::filesystem {
        public:
            enum class device_class {
                CHARDEV,
                BLOCKDEV,
                BUS,
                OTHER
            };

            struct device;
            struct busdev;

            struct matcher {
                bool has_file;
                bool single;

                const char *base_name;
                const char *subdir;
                bool alpha_names;
                size_t start_index;

                virtual device *match(devfs::busdev *bus, void *aux) { return nullptr; }
                virtual void attach(devfs::busdev *bus, devfs::device *dev, void *aux) { return; }

                matcher(bool has_file, bool single, const char *base_name, const char *subdir,
                        bool alpha_names, size_t start_index):
                    has_file(has_file), single(single), base_name(base_name), subdir(subdir), alpha_names(alpha_names), start_index(0) {}
                
                virtual ~matcher() {}
            };

            struct bus_space {
                protected:
                    bus_addr_t addr;
                    bus_size_t size;
                    bool linear;
                public:
                    bus_space() = delete;

                    bus_space(bus_addr_t addr, bus_size_t size, bool linear)
                        : addr(addr), size(size), linear(linear) {}
                    virtual ~bus_space() {}

                    virtual bool is_linear() = 0;

                    virtual bus_handle_t map(bus_addr_t offset, bus_size_t size) = 0;
                    virtual void unmap(bus_handle_t handle) = 0;
                    virtual void *vaddr(bus_handle_t handle) = 0;

                    virtual uint8_t readb(bus_handle_t handle, bus_size_t offset) = 0;
                    virtual uint16_t readw(bus_handle_t handle, bus_size_t offset) = 0;
                    virtual uint32_t readd(bus_handle_t handle, bus_size_t offset) = 0;
                    virtual uint64_t readq(bus_handle_t handle, bus_size_t offset) = 0;

                    virtual void writeb(bus_handle_t handle, bus_size_t offset, uint8_t val) = 0;
                    virtual void writew(bus_handle_t handle, bus_size_t offset, uint16_t val) = 0;
                    virtual void writed(bus_handle_t handle, bus_size_t offset, uint32_t val) = 0;
                    virtual void writeq(bus_handle_t handle, bus_size_t offset, uint64_t val) = 0;

                    virtual void read_regionb(bus_handle_t handle, bus_size_t offset, uint8_t *data, size_t count) = 0;
                    virtual void read_regionw(bus_handle_t handle, bus_size_t offset, uint16_t *data, size_t count) = 0;
                    virtual void read_regiond(bus_handle_t handle, bus_size_t offset, uint32_t *data, size_t count) = 0;
                    virtual void read_regionq(bus_handle_t handle, bus_size_t offset, uint64_t *data, size_t count) = 0;

                    virtual void write_regionb(bus_handle_t handle, bus_size_t offset, uint8_t *data, size_t count) = 0;
                    virtual void write_regionw(bus_handle_t handle, bus_size_t offset, uint16_t *data, size_t count) = 0;
                    virtual void write_regiond(bus_handle_t handle, bus_size_t offset, uint32_t *data, size_t count) = 0;
                    virtual void write_regionq(bus_handle_t handle, bus_size_t offset, uint64_t *data, size_t count) = 0;

                    virtual void set_regionb(bus_handle_t handle, bus_size_t offset, uint8_t val, size_t count) = 0;
                    virtual void set_regionw(bus_handle_t handle, bus_size_t offset, uint16_t val, size_t count) = 0;
                    virtual void set_regiond(bus_handle_t handle, bus_size_t offset, uint32_t val, size_t count) = 0;
                    virtual void set_regionq(bus_handle_t handle, bus_size_t offset, uint64_t val, size_t count) = 0;

                    virtual void read_multib(bus_handle_t handle, bus_size_t offset, uint8_t *data, size_t count) = 0;
                    virtual void read_multiw(bus_handle_t handle, bus_size_t offset, uint16_t *data, size_t count) = 0;
                    virtual void read_multid(bus_handle_t handle, bus_size_t offset, uint32_t *data, size_t count) = 0;
                    virtual void read_multiq(bus_handle_t handle, bus_size_t offset, uint64_t *data, size_t count) = 0;

                    virtual void write_multib(bus_handle_t handle, bus_size_t offset, uint8_t *data, size_t count) = 0;
                    virtual void write_multiw(bus_handle_t handle, bus_size_t offset, uint16_t *data, size_t count) = 0;
                    virtual void write_multid(bus_handle_t handle, bus_size_t offset, uint32_t *data, size_t count) = 0;
                    virtual void write_multiq(bus_handle_t handle, bus_size_t offset, uint64_t *data, size_t count) = 0;
            };

            struct bus_dma {
                protected:
                    bus_addr_t addr;
                    bus_size_t len;
                public:
                bus_dma(bus_size_t len):len(len) {}
                virtual ~bus_dma() {};

                virtual bus_addr_t vaddr() = 0;
                virtual bus_addr_t paddr() = 0;

                virtual bus_addr_t map(void *vaddr) = 0;
                virtual void unmap(bus_addr_t paddr) = 0;
            };

            struct device {
                prs::allocator allocator;

                prs::vector<device *, prs::allocator> bus_devices;
                devfs::busdev *bus;

                ssize_t major;
                ssize_t minor;

                device_class cls;
                device(devfs::busdev *bus, ssize_t major, ssize_t minor, void *aux, device_class cls): 
                    allocator(arena::create_resource()), 
                    bus_devices(allocator), bus(bus), major(major), minor(minor), cls(cls) {};

                // virtual void *mmap(node *file, void *addr, size_t len, size_t offset) { return nullptr; }
            };

            struct busdev: device {
                virtual void enumerate() = 0;
                virtual void attach(ssize_t major, void *aux) = 0;

                virtual shared_ptr<bus_dma> get_dma(size_t size) { return nullptr; }

                busdev(devfs::busdev *bus, ssize_t major, ssize_t minor, void *aux): device(bus, major, minor, aux, devfs::device_class::BUS) { }
                virtual ~busdev() {}
            };

            struct filedev: device {
                protected:
                    virtual ssize_t arise(ssize_t event) = 0;
                public:
                    prs::vector<shared_ptr<poll::producer>, prs::allocator> outputs;
                        shared_ptr<node> file;

                    virtual ssize_t on_open(shared_ptr<fd> fd, ssize_t flags) = 0;
                    virtual ssize_t on_close(shared_ptr<fd> fd, ssize_t flags) = 0;

                    virtual ssize_t read(void *buf, size_t len, size_t offset) = 0;
                    virtual ssize_t write(void *buf, size_t len, size_t offset) = 0;
                    virtual ssize_t ioctl(size_t req, void *buf) = 0;

                    virtual ssize_t poll(shared_ptr<poll::producer> producer) = 0;

                    virtual ssize_t force_dismount() = 0;

                    filedev(devfs::busdev *bus, ssize_t major, ssize_t minor, void *aux, device_class cls): device(bus, major, minor, aux, cls), outputs(allocator) {};
                    virtual ~filedev() {}
            };

            struct blockdev: filedev {
                protected:
                    virtual ssize_t arise(ssize_t event) override { return -ENOTSUP; };
                public:
                    static constexpr size_t defaultReadSize = memory::page_size * 64;
                    struct partition {
                        size_t blocks;
                        size_t begin;
                        partition(size_t blocks, size_t begin) : blocks(blocks), begin(begin) { };
                    };
                    
                    prs::vector<partition, prs::allocator> part_list;
                    prs::vector<filesystem *, prs::allocator> fs_list;

                    size_t blocks;
                    size_t block_size;
                    
                    cache::holder *disk_cache;

                    virtual ssize_t on_open(shared_ptr<fd> fd, ssize_t flags) override { return -ENOTSUP; }
                    virtual ssize_t on_close(shared_ptr<fd> fd, ssize_t flags) override { return -ENOTSUP; }

                    virtual ssize_t read(void *buf, size_t len, size_t offset) override { return 0; }
                    virtual ssize_t write(void *buf, size_t len, size_t offset) override { return 0; }
                    virtual ssize_t ioctl(size_t req, void *buf) override { return 0; }

                    virtual ssize_t poll(shared_ptr<poll::producer> producer) override { return POLLIN | POLLOUT; }

                    virtual ssize_t force_dismount() override { return -ENOTSUP; }

                    blockdev(devfs::busdev *bus, ssize_t major, ssize_t minor, void *aux): 
                        filedev(bus, major, minor, aux, devfs::device_class::BLOCKDEV),
                        part_list(allocator), fs_list(allocator) {}
                    virtual ~blockdev() {}
            };

            struct chardev: filedev {
                protected:
                    ssize_t arise(ssize_t event) override;                
                public:
                    virtual ssize_t on_open(shared_ptr<fd> fd, ssize_t flags) override { return 0; }
                    virtual ssize_t on_close(shared_ptr<fd> fd, ssize_t flags) override { return 0; }

                    virtual ssize_t read(void *buf, size_t len, size_t offset) override { return 0; }
                    virtual ssize_t write(void *buf, size_t len, size_t offset) override { return 0; }
                    virtual ssize_t ioctl(size_t req, void *buf) override { return 0; }

                    virtual ssize_t poll(shared_ptr<poll::producer> producer) override { return 0; }

                    virtual ssize_t force_dismount() override { return -ENOTSUP; }

                    chardev(devfs::busdev *bus, ssize_t major, ssize_t minor, void *aux): filedev(bus, major, minor, aux, devfs::device_class::CHARDEV) {}
                    virtual ~chardev() {}
            };

            struct dev_priv {
                device *dev;
                int part;
            };

            static constexpr size_t MAINBUS_MAJOR = 0xAF;
            struct rootbus: busdev {
                void enumerate() override;
                void attach(ssize_t major, void *aux) override;

                rootbus(): vfs::devfs::busdev(nullptr, MAINBUS_MAJOR, -1, nullptr) {};
            };

            static rootbus *mainbus;

            static void init();
            static void probe();

            struct device_list {
                prs::vector<device *, prs::allocator> list;
                size_t last_index;

                device_list(): list(arena::create_resource()), last_index(0) {}
            };

            inline static frg::hash_map<
                size_t,
                device_list,
                frg::hash<size_t>,
                prs::allocator>
            device_map{frg::hash<size_t>(), arena::create_resource()};

            static void append_device(device *dev, ssize_t major);
            static void remove_device(device *dev, ssize_t major);

            /*
            filesystem(shared_ptr<ns::mount> ns, weak_ptr<filesystem> self,
                shared_ptr<node> root, weak_ptr<node> device):            
            */
            devfs(shared_ptr<ns::mount> ns,
                shared_ptr<node> root):
                vfs::filesystem(ns, root, {}) { };

            weak_ptr<node> lookup(shared_ptr<node> parent, prs::string_view name) override;

            ssize_t on_open(shared_ptr<fd> fd, ssize_t flags) override;
            ssize_t on_close(shared_ptr<fd> fd, ssize_t flags) override;

            ssize_t read(shared_ptr<node> file, void *buf, size_t len, off_t offset) override;
            ssize_t write(shared_ptr<node> file, void *buf, size_t len, off_t offset) override;
            ssize_t ioctl(shared_ptr<node> file, size_t req, void *buf) override;
            void *mmap(shared_ptr<node> file, void *addr, size_t len, off_t offset) override;
            ssize_t poll(shared_ptr<descriptor> file) override;
            ssize_t mkdir(shared_ptr<node> dst, prs::string_view name, int64_t flags, mode_t mode,
                uid_t uid, gid_t gid) override;
    };
};

#endif