// license:BSD-3-Clause
// copyright-holders:Ted Green
#include "pci-ide.h"

const device_type IDE_PCI = &device_creator<ide_pci_device>;

ide_pci_device::ide_pci_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: pci_device(mconfig, IDE_PCI, "IDE PCI interface", tag, owner, clock, "ide_pci", __FILE__),
	m_ide(*this, "ide"),
	m_ide2(*this, "ide2"),
	m_irq_num(-1),
	m_irq_handler(*this)
{
}

DEVICE_ADDRESS_MAP_START(config_map, 32, ide_pci_device)
	AM_RANGE(0x08, 0x0b) AM_WRITE8(prog_if_w, 0x0000ff00)
	AM_RANGE(0x10, 0x1f) AM_WRITE(address_base_w)
	AM_RANGE(0x40, 0x5f) AM_READWRITE(pcictrl_r, pcictrl_w)
	AM_RANGE(0x70, 0x77) AM_DEVREADWRITE("ide", bus_master_ide_controller_device, bmdma_r, bmdma_w) // PCI646
	AM_RANGE(0x78, 0x7f) AM_DEVREADWRITE("ide2", bus_master_ide_controller_device, bmdma_r, bmdma_w) // PCI646
	AM_INHERIT_FROM(pci_device::config_map)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(chan1_data_command_map, 32, ide_pci_device)
	AM_RANGE(0x0, 0x7) AM_DEVREADWRITE("ide", bus_master_ide_controller_device, read_cs0, write_cs0)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(chan1_control_map, 32, ide_pci_device)
	AM_RANGE(0x0, 0x3) AM_READWRITE(ide_read_cs1, ide_write_cs1)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(chan2_data_command_map, 32, ide_pci_device)
	AM_RANGE(0x0, 0x7) AM_DEVREADWRITE("ide2", bus_master_ide_controller_device, read_cs0, write_cs0)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(chan2_control_map, 32, ide_pci_device)
	AM_RANGE(0x0, 0x3) AM_READWRITE(ide2_read_cs1, ide2_write_cs1)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(bus_master_map, 32, ide_pci_device)
	AM_RANGE(0x0, 0x7) AM_DEVREADWRITE("ide", bus_master_ide_controller_device, bmdma_r, bmdma_w)
	AM_RANGE(0x8, 0xf) AM_DEVREADWRITE("ide2", bus_master_ide_controller_device, bmdma_r, bmdma_w)
ADDRESS_MAP_END

static MACHINE_CONFIG_FRAGMENT(pci_ide)
	MCFG_BUS_MASTER_IDE_CONTROLLER_ADD("ide", ata_devices, "hdd", "cdrom", true)
	MCFG_ATA_INTERFACE_IRQ_HANDLER(WRITELINE(ide_pci_device, ide_interrupt))
	//MCFG_BUS_MASTER_IDE_CONTROLLER_SPACE(":maincpu", AS_PROGRAM)
	MCFG_BUS_MASTER_IDE_CONTROLLER_SPACE(":pci:00.0", AS_DATA)
	MCFG_BUS_MASTER_IDE_CONTROLLER_ADD("ide2", ata_devices, "hdd", "cdrom", true)
	MCFG_ATA_INTERFACE_IRQ_HANDLER(WRITELINE(ide_pci_device, ide_interrupt))
	//MCFG_BUS_MASTER_IDE_CONTROLLER_SPACE(":maincpu", AS_PROGRAM)
	MCFG_BUS_MASTER_IDE_CONTROLLER_SPACE(":pci:00.0", AS_DATA)
MACHINE_CONFIG_END

machine_config_constructor ide_pci_device::device_mconfig_additions() const
{
	return MACHINE_CONFIG_NAME(pci_ide);
}

void ide_pci_device::set_irq_info(const char *tag, const int irq_num)
{
	m_cpu_tag = tag;
	m_irq_num = irq_num;
}

void ide_pci_device::device_start()
{
	if (m_irq_num != -1)
		m_cpu = machine().device<cpu_device>(m_cpu_tag);

	pci_device::device_start();

	add_map(8,    M_IO,  FUNC(ide_pci_device::chan1_data_command_map));
	bank_infos[0].adr = 0x1f0;
	add_map(4,    M_IO,  FUNC(ide_pci_device::chan1_control_map));
	bank_infos[1].adr = 0x3f4;
	add_map(8,    M_IO,  FUNC(ide_pci_device::chan2_data_command_map));
	bank_infos[2].adr = 0x170;
	add_map(4,    M_IO,  FUNC(ide_pci_device::chan2_control_map));
	bank_infos[3].adr = 0x374;
	add_map(16,   M_IO,  FUNC(ide_pci_device::bus_master_map));
	bank_infos[4].adr = 0xf00;

	// Setup stored BARs
	pci_bar[0] = 0x1f0;
	pci_bar[1] = 0x3f4;
	pci_bar[2] = 0x170;
	pci_bar[3] = 0x374;
	pci_bar[4] = 0xf00;

	m_irq_handler.resolve_safe();

	intr_pin = 0x1;
	intr_line = 0xe;
}

void ide_pci_device::device_reset()
{
	pci_device::device_reset();
	// PCI0646U allow BAR
	if (main_id == 0x10950646)
		m_config_data[0x10 / 4] |= 0x0C40;
}

READ32_MEMBER(ide_pci_device::ide_read_cs1)
{
	// PCI offset starts at 0x3f4, idectrl expects 0x3f0
	uint32_t data;
	data = m_ide->read_cs1(space, 1, mem_mask);
	if (0)
		logerror("%s:ide_read_cs1 offset=%08X data=%08X mask=%08X\n", machine().describe_context(), offset, data, mem_mask);
	return data;
}

WRITE32_MEMBER(ide_pci_device::ide_write_cs1)
{
	// PCI offset starts at 0x3f4, idectrl expects 0x3f0
	m_ide->write_cs1(space, 1, data, mem_mask);
}

READ32_MEMBER(ide_pci_device::ide2_read_cs1)
{
	// PCI offset starts at 0x374, idectrl expects 0x370
	uint32_t data;
	data = m_ide2->read_cs1(space, 1, mem_mask);
	return data;
}

WRITE32_MEMBER(ide_pci_device::ide2_write_cs1)
{
	// PCI offset starts at 0x374, idectrl expects 0x370
	m_ide2->write_cs1(space, 1, data, mem_mask);
}

WRITE_LINE_MEMBER(ide_pci_device::ide_interrupt)
{
	// Assert/Clear the interrupt if the irq num is set.
	if (m_irq_num != -1) {
		m_cpu->set_input_line(m_irq_num, state);
	}
	// Call the callback
	m_irq_handler(state);

	// PCI646U2 Offset 0x50 is interrupt status
	if (main_id == 0x10950646) {
		if (state)
			m_config_data[0x10 / 4] |= 0x4;
		else
			m_config_data[0x10 / 4] &= ~0x4;
	}
	if (0)
		logerror("%s:ide_interrupt %i set to %i\n", machine().describe_context(), m_irq_num, state);
}

WRITE8_MEMBER(ide_pci_device::prog_if_w)
{
	uint32_t oldVal = pclass;
	pclass = (pclass & ~(0xff)) | (data & 0xff);
	// Check for switch to/from compatibility (legacy) mode from/to pci mode
	if ((oldVal ^ pclass) & 0x5) {
		// Map Primary IDE Channel
		if (pclass & 0x1) {
			// PCI Mode
			// Enabling BAR 4 in legacy mode
			bank_infos[4].flags &= ~M_DISABLED;
			pci_device::address_base_w(space, 0, pci_bar[0]);
			pci_device::address_base_w(space, 1, pci_bar[1]);
		} else {
			// Legacy Mode
			// Disabling BAR 4 in legacy mode
			bank_infos[4].flags |= M_DISABLED;
			pci_device::address_base_w(space, 0, 0x1f0);
			pci_device::address_base_w(space, 1, 0x3f4);
		}
		// Map Primary IDE Channel
		if (pclass & 0x4) {
			// PCI Mode
			pci_device::address_base_w(space, 2, pci_bar[2]);
			pci_device::address_base_w(space, 3, pci_bar[3]);
		}
		else {
			// Legacy Mode
			pci_device::address_base_w(space, 2, 0x170);
			pci_device::address_base_w(space, 3, 0x374);
		}
	}
	if (1)
		logerror("%s:prog_if_w pclass = %06X\n", machine().describe_context(), pclass);
}

READ32_MEMBER(ide_pci_device::pcictrl_r)
{
	return m_config_data[offset];
}

WRITE32_MEMBER(ide_pci_device::pcictrl_w)
{
	COMBINE_DATA(&m_config_data[offset]);
	// PCI646U2 Offset 0x50 is interrupt status
	if (main_id == 0x10950646 && offset == (0x10 / 4) && (data & 0x4)) {
		m_config_data[0x10 / 4] &= ~0x4;
		if (0)
			logerror("%s:ide_pci_device::pcictrl_w Clearing interrupt status\n", machine().describe_context());
	}
}

WRITE32_MEMBER(ide_pci_device::address_base_w)
{
	// data==0xffffffff is used to identify required memory space
	if (data != 0xffffffff) {
		// Save local copy of BAR
		pci_bar[offset] = data;
		// Bits 0 (primary) and 2 (secondary) control if the mapping is legacy or BAR
		switch (offset) {
		case 0: case 1:
			if ((pclass & 0x1) == 1)
				pci_device::address_base_w(space, offset, data);
			break;
		case 2: case 3:
			if ((pclass & 0x4) == 1)
				pci_device::address_base_w(space, offset, data);
			break;
		default:
			pci_device::address_base_w(space, offset, data);
			// Not sure what to do for the bus master ide BAR in legacy mode
			// prog_if_w will disable register in legacy mode
			if ((pclass & 0x5) == 0)
				logerror("Mapping bar[%i] in legacy mode\n", offset);
		}
	}
}
