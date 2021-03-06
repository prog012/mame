// license:BSD-3-Clause
// copyright-holders:hap
/*

  Hughes HLCD 0515/0569 LCD Driver
  
  0515: 25 columns(also size of buffer/ram)
  0569: 24 columns, no DATA OUT pin, display blank has no effect

  TODO:
  - read mode is untested
  - MAME bitmap update callback when needed

*/

#include "video/hlcd0515.h"


const device_type HLCD0515 = &device_creator<hlcd0515_device>;
const device_type HLCD0569 = &device_creator<hlcd0569_device>;

//-------------------------------------------------
//  constructor
//-------------------------------------------------

hlcd0515_device::hlcd0515_device(const machine_config &mconfig, device_type type, const char *name, const char *tag, device_t *owner, u32 clock, u8 colmax, const char *shortname, const char *source)
	: device_t(mconfig, type, name, tag, owner, clock, shortname, source),
	m_colmax(colmax), m_write_cols(*this), m_write_data(*this)
{
}

hlcd0515_device::hlcd0515_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: hlcd0515_device(mconfig, HLCD0515, "HLCD 0515 LCD Driver", tag, owner, clock, 25, "hlcd0515", __FILE__)
{
}

hlcd0569_device::hlcd0569_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: hlcd0515_device(mconfig, HLCD0569, "HLCD 0569 LCD Driver", tag, owner, clock, 24, "hlcd0569", __FILE__)
{
}



//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void hlcd0515_device::device_start()
{
	// resolve callbacks
	m_write_cols.resolve_safe();
	m_write_data.resolve_safe();

	// timer
	m_lcd_timer = timer_alloc();
	m_lcd_timer->adjust(attotime::from_hz(clock() / 2), 0, attotime::from_hz(clock() / 2));

	// zerofill
	m_cs = 0;
	m_clock = 0;
	m_data = 0;
	m_count = 0;
	m_control = 0;
	m_blank = false;
	m_rowmax = 0;
	m_rowout = 0;
	m_rowsel = 0;
	m_buffer = 0;
	memset(m_ram, 0, sizeof(m_ram));

	// register for savestates
	save_item(NAME(m_cs));
	save_item(NAME(m_clock));
	save_item(NAME(m_data));
	save_item(NAME(m_count));
	save_item(NAME(m_control));
	save_item(NAME(m_blank));
	save_item(NAME(m_rowmax));
	save_item(NAME(m_rowout));
	save_item(NAME(m_rowsel));
	save_item(NAME(m_buffer));
	save_item(NAME(m_ram));
}



//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void hlcd0515_device::device_reset()
{
}



//-------------------------------------------------
//  device_timer - handler timer events
//-------------------------------------------------

void hlcd0515_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	if (m_rowout > m_rowmax)
		m_rowout = 0;

	// write to COL/ROW pins
	m_write_cols(m_rowout, m_blank ? 0 : m_ram[m_rowout], 0xffffffff);
	m_rowout++;
}



//-------------------------------------------------
//  handlers
//-------------------------------------------------

void hlcd0515_device::set_control()
{
	// clock 0,1,2: row select
	m_rowsel = m_control >> 2 & 7;

	// clock 3(,4): initialize
	if (m_control & 2)
	{
		m_rowmax = m_rowsel;
		m_blank = bool(~m_control & 1);
	}

	// clock 4: read/write mode
	m_buffer = (m_control & 1) ? m_ram[m_rowsel] : 0;
}

void hlcd0569_device::set_control()
{
	hlcd0515_device::set_control();
	m_blank = false; // 0569 doesn't support display blanking
}


void hlcd0515_device::clock_data(int col)
{
	if (m_control & 1)
	{
		if (col < m_colmax)
			m_buffer <<= 1;

		m_write_data(m_buffer >> m_colmax & 1);
	}
	else
	{
		if (col < m_colmax)
			m_buffer >>= 1;

		// always write last column
		u32 mask = 1 << (m_colmax - 1);
		m_buffer = (m_buffer & ~mask) | (m_data ? mask : 0);
	}
}


WRITE_LINE_MEMBER(hlcd0515_device::write_clock)
{
	state = (state) ? 1 : 0;

	// clock/shift data on falling edge
	if (!m_cs && !state && m_clock)
	{
		if (m_count < 5)
		{
			// 5-bit mode/control
			m_control = m_control << 1 | m_data;
			if (m_count == 4)
				set_control();
		}

		else
			clock_data(m_count - 5);
		
		if (m_count < (m_colmax + 5))
			m_count++;
	}

	m_clock = state;
}


WRITE_LINE_MEMBER(hlcd0515_device::write_cs)
{
	state = (state) ? 1 : 0;

	// finish serial sequence on rising edge
	if (state && !m_cs)
	{
		// transfer to ram
		if (~m_control & 1)
			m_ram[m_rowsel] = m_buffer;
		
		m_count = 0;
		m_control = 0;
	}

	m_cs = state;
}
