#ifndef PCI_HPP
#define PCI_HPP

#include <cstddef>
#include <cstdint>
#include <frg/vector.hpp>
#include <mm/mm.hpp>

namespace pci {
    static constexpr auto CONFIG_PORT = 0xCF8;
    static constexpr auto DATA_PORT   = 0xCFC;
    
    static constexpr auto PCI_HAS_CAPS = 0x4;
    static constexpr auto PCI_CAPS     = 0x34;
    static constexpr auto MSI_OPT      = 0x2;
    static constexpr auto MSI_ADDR_LOW = 0x4;
    static constexpr auto MSI_DATA_32  = 0x8;
    static constexpr auto MSI_DATA_64  = 0xC;
    static constexpr auto MSI_64BIT_SUPPORTED = (1 << 7);

    static constexpr size_t MAX_FUNCTION = 8;
    static constexpr size_t MAX_DEVICE   = 32;
    static constexpr size_t MAX_BUS      = 256;

    struct bar {
        size_t base;
        size_t size;
        bool is_mmio;
        bool is_prefetchable;
        bool valid;
    };

    class device {
        private:
            uint8_t
                bus,
                slot,
                clazz,
                subclass,
                prog_if,
                func;

            uint16_t
                devize,
                vendor_id;
            bool is_multifunc;

        public:
            device(uint8_t bus, uint8_t slot, uint8_t func, uint8_t clazz, uint8_t subclass,
                                uint8_t prog_if,
                                uint16_t device, uint16_t vendor_id,
                                bool is_multifunc) {
                this->bus      = bus;
                this->slot     = slot;
                this->func     = func;
                this->clazz    = clazz;
                this->subclass = subclass;
                this->prog_if  = prog_if;

                this->devize = device;
                this->vendor_id = vendor_id;
                this->is_multifunc = is_multifunc;
            }

            uint8_t get_bus(),
                    get_slot(),
                    get_clazz(),
                    get_subclass(),
                    get_prog_if(),
                    get_func();
                    
            uint16_t
                    get_vendor(),
                    get_device();

            uint8_t  readb(uint32_t offset);
            void     writeb(uint32_t offset, uint8_t value);
            uint16_t readw(uint32_t offset);
            void     writew(uint32_t offset, uint16_t value);
            uint32_t readd(uint32_t offset);
            void     writed(uint32_t offset, uint32_t value);
            int      read_bar(size_t index, pci::bar& bar_out);
            int      register_msi(uint8_t vector, uint8_t lapic_id);

            void     enable_busmastering();
            void     enable_mmio();
            uint8_t  read_irq();
    };

    union [[gnu::packed]] msi_address {
        struct {
            uint32_t resv0 : 2;
            uint32_t dest_mode : 1;
            uint32_t redir_hint : 1;
            uint32_t resv1 : 8;
            uint32_t dest_id : 8;
            uint32_t base_addr : 12;
        };
        uint32_t raw;
    };

    union [[gnu::packed]] msi_data {
        struct {
            uint32_t vector : 8;
            uint32_t delv_mode : 3;
            uint32_t resv0 : 3;
            uint32_t level : 1;
            uint32_t trig_mode : 1;
            uint32_t resv1 : 16;
        };
        uint32_t raw;
    };
    
    void init();
    device *get_device(uint8_t clazz, uint8_t subclazz, uint8_t prog_if);
    device *get_device(uint16_t vendor, uint16_t device);
    const char *to_string(uint8_t clazz, uint8_t subclass, uint8_t prog_if);
};

#endif