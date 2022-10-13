// @file Utf8_16.cxx
// Copyright (C) 2002 Scott Kirkwood
//
// Permission to use, copy, modify, distribute and sell this code
// and its documentation for any purpose is hereby granted without fee,
// provided that the above copyright notice appear in all copies or
// any derived copies.  Scott Kirkwood makes no representations
// about the suitability of this software for any purpose.
// It is provided "as is" without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstring>
#include <cstdio>

#include <memory>

#include "Utf8_16.h"

const Utf8_16::utf8 Utf8_16::k_Boms[][3] = {
	{0x00, 0x00, 0x00},  // Unknown
	{0xFE, 0xFF, 0x00},  // Big endian
	{0xFF, 0xFE, 0x00},  // Little endian
	{0xEF, 0xBB, 0xBF}, // UTF8
};

enum { SURROGATE_LEAD_FIRST = 0xD800 };
enum { SURROGATE_LEAD_LAST = 0xDBFF };
enum { SURROGATE_TRAIL_FIRST = 0xDC00 };
enum { SURROGATE_TRAIL_LAST = 0xDFFF };
enum { SURROGATE_FIRST_VALUE = 0x10000 };

namespace {

// Reads UTF-8 and outputs UTF-16
class Utf8_Iter : public Utf8_16 {
public:
	Utf8_Iter() noexcept;
	void set(const ubyte *pBuf, size_t nLen, encodingType eEncoding) noexcept;
	int get() const noexcept {
		assert(m_eState == eStart);
		return m_nCur;
	}
	bool canGet() const noexcept { return m_eState == eStart; }
	void operator++() noexcept;
	operator bool() const noexcept { return m_pRead <= m_pEnd; }

protected:
	void toStart() noexcept; // Put to start state
	enum eState {
		eStart,
		eSecondOf4Bytes,
		ePenultimate,
		eFinal
	};
protected:
	encodingType m_eEncoding;
	eState m_eState;
	int m_nCur;
	// These 3 pointers are for externally allocated memory passed to set
	const ubyte *m_pBuf;
	const ubyte *m_pRead;
	const ubyte *m_pEnd;
};

}

// ==================================================================

Utf8_16_Read::Utf8_16_Read() noexcept {
	m_eEncoding = eUnknown;
	m_nBufSize = 0;
	m_pBuf = nullptr;
	m_pNewBuf = nullptr;
	m_bFirstRead = true;
	m_nLen = 0;
	m_leadSurrogate[0] = 0;
	m_leadSurrogate[1] = 0;
#ifdef RB_UTF8AC
	m_nAutoCheckUtf8 = false;//!-add-[utf8.auto.check]
#endif
}

#ifdef RB_UTF8AC
//!-start-[utf8.auto.check]
Utf8_16_Read::Utf8_16_Read(bool AutoCheckUtf8) noexcept {
	m_eEncoding = eUnknown;
	m_nBufSize = 0;
	m_pNewBuf = NULL;
	m_bFirstRead = true;
	m_nAutoCheckUtf8 = AutoCheckUtf8;
}

//[mhb] 07/05/09 : check whether a data block contains UTF8 chars
int Has_UTF8_Char(unsigned char* buf, size_t size) {
	if (!buf || size < 2) { return 0; }
	unsigned char* p = buf;
	int i = 0, cnt = 0;
	while (p[0] && i < size) {
		if (p[0] >> 7 == 0x01) {
			if (p[0] >> 5 == 0x06 && (i + 1) < size) {
				if (p[1] >> 6 != 0x02) { return 0; } //not utf8
				cnt++; p++; i++;//UTF-8: U-00000080 ?C U-000007FF
			}
			else if (p[0] >> 4 == 0x0e && (i + 2) < size) {
				if (p[1] >> 6 != 0x02) { return 0; } //not utf8
				if (p[2] >> 6 != 0x02) { return 0; } //not utf8
				cnt++; p += 2; i += 2;//UTF-8: U-00000800 ?C U-0000FFFF
			}
			else if (p[0] >> 3 == 0x1e && (i + 3) < size) {
				if (p[1] >> 6 != 0x02) { return 0; } //not utf8
				if (p[2] >> 6 != 0x02) { return 0; } //not utf8
				if (p[3] >> 6 != 0x02) { return 0; } //not utf8
				cnt++; p += 3; i += 3;//UTF-8: U-00010000 ?C U-001FFFFF
			}
			else if (p[0] >> 2 == 0x3e && (i + 4) < size) {
				if (p[1] >> 6 != 0x02) { return 0; } //not utf8
				if (p[2] >> 6 != 0x02) { return 0; } //not utf8
				if (p[3] >> 6 != 0x02) { return 0; } //not utf8
				if (p[4] >> 6 != 0x02) { return 0; } //not utf8
				cnt++; p += 4; i += 4;//UTF-8: U-00200000 ?C U-03FFFFFF
			}
			else if (p[0] >> 1 == 0x7e && (i + 5) < size) {
				if (p[1] >> 6 != 0x02) { return 0; } //not utf8
				if (p[2] >> 6 != 0x02) { return 0; } //not utf8
				if (p[3] >> 6 != 0x02) { return 0; } //not utf8
				if (p[4] >> 6 != 0x02) { return 0; } //not utf8
				if (p[5] >> 6 != 0x02) { return 0; } //not utf8
				cnt++; p += 5; i += 5;//UTF-8: U-04000000 ?C U-7FFFFFFF
			}
		}
		p++; i++;
	}
	return cnt > 0 ? 1 : 0;
}
//!-end-[utf8.auto.check]
#endif

Utf8_16_Read::~Utf8_16_Read() noexcept {
	if ((m_eEncoding != eUnknown) && (m_eEncoding != eUtf8)) {
		delete [] m_pNewBuf;
		m_pNewBuf = nullptr;
	}
}

size_t Utf8_16_Read::convert(char *buf, size_t len) {
	m_pBuf = reinterpret_cast<ubyte *>(buf);
	m_nLen = len;

	int nSkip = 0;
	if (m_bFirstRead) {
		nSkip = determineEncoding();
		m_bFirstRead = false;
	}

	if (m_eEncoding == eUnknown) {
		// Do nothing, pass through
		m_nBufSize = 0;
		m_pNewBuf = m_pBuf;
		return len;
	}

	if (m_eEncoding == eUtf8) {
		// Pass through after BOM
		m_nBufSize = 0;
		m_pNewBuf = m_pBuf + nSkip;
		return len - nSkip;
	}

	// Else...
	const size_t newSize = len + len / 2 + 4 + 1;
	if (m_nBufSize != newSize) {
		delete [] m_pNewBuf;
		m_pNewBuf = new ubyte[newSize];
		m_nBufSize = newSize;
	}

	ubyte *pCur = m_pNewBuf;

	ubyte endSurrogate[2] = { 0, 0 };
	ubyte *pbufPrependSurrogate = nullptr;
	if (m_leadSurrogate[0]) {
		pbufPrependSurrogate = new ubyte[len - nSkip + 2];
		memcpy(pbufPrependSurrogate, m_leadSurrogate, 2);
		if (m_pBuf)
			memcpy(pbufPrependSurrogate + 2, m_pBuf + nSkip, len - nSkip);
		m_Iter16.set(pbufPrependSurrogate, len - nSkip + 2, m_eEncoding, endSurrogate);
	} else {
		if (!m_pBuf)
			return 0;
		m_Iter16.set(m_pBuf + nSkip, len - nSkip, m_eEncoding, endSurrogate);
	}

	for (; m_Iter16; ++m_Iter16) {
		*pCur++ = m_Iter16.get();
	}

	delete []pbufPrependSurrogate;

	memcpy(m_leadSurrogate, endSurrogate, 2);

	// Return number of bytes written out
	return pCur - m_pNewBuf;
}

int Utf8_16_Read::determineEncoding() noexcept {
	m_eEncoding = eUnknown;

	int nRet = 0;

	if (m_nLen > 1) {
		if (m_pBuf[0] == k_Boms[eUtf16BigEndian][0] && m_pBuf[1] == k_Boms[eUtf16BigEndian][1]) {
			m_eEncoding = eUtf16BigEndian;
			nRet = 2;
		} else if (m_pBuf[0] == k_Boms[eUtf16LittleEndian][0] && m_pBuf[1] == k_Boms[eUtf16LittleEndian][1]) {
			m_eEncoding = eUtf16LittleEndian;
			nRet = 2;
		} else if (m_nLen > 2 && m_pBuf[0] == k_Boms[eUtf8][0] && m_pBuf[1] == k_Boms[eUtf8][1] && m_pBuf[2] == k_Boms[eUtf8][2]) {
			m_eEncoding = eUtf8;
			nRet = 3;
		}
#ifdef RB_UTF8AC
		//!-start-[utf8.auto.check]
		//[mhb] 07/05/09 :to support checking utf-8 from raw chars; 07/07/09 : method 1
		else if (m_nLen > 2 && m_nAutoCheckUtf8 == 1) {
			if (Has_UTF8_Char(m_pBuf, m_nLen)) {
				m_eEncoding = eUtf8;
				nRet = 0;
			}
		}
		//!-end-[utf8.auto.check]
#endif // RB_UTF8AC

	}

	return nRet;
}

// ==================================================================

Utf8_16_Write::Utf8_16_Write() noexcept {
	m_eEncoding = eUnknown;
	m_pFile = nullptr;
	m_bFirstWrite = true;
	m_nBufSize = 0;
}

Utf8_16_Write::~Utf8_16_Write() noexcept {
	if (m_pFile) {
		fclose();
	}
}

void Utf8_16_Write::setfile(FILE *pFile) noexcept {
	m_pFile = pFile;

	m_bFirstWrite = true;
}

// Swap the two low order bytes of an integer value
static int swapped(int v) noexcept {
	return ((v & 0xFF) << 8) + (v >> 8);
}

size_t Utf8_16_Write::fwrite(const void *p, size_t _size) {
	if (!m_pFile) {
		return 0; // fail
	}

	if (m_eEncoding == eUnknown) {
		// Normal write
		return ::fwrite(p, _size, 1, m_pFile);
	}

	if (m_eEncoding == eUtf8) {
		if (m_bFirstWrite)
			::fwrite(k_Boms[m_eEncoding], 3, 1, m_pFile);
		m_bFirstWrite = false;
		return ::fwrite(p, _size, 1, m_pFile);
	}

	if (_size > m_nBufSize) {
		m_nBufSize = _size;
		m_buf16 = std::make_unique<utf16[]>(m_nBufSize + 1);
	}

	if (m_bFirstWrite) {
		if (m_eEncoding == eUtf16BigEndian || m_eEncoding == eUtf16LittleEndian) {
			// Write the BOM
			::fwrite(k_Boms[m_eEncoding], 2, 1, m_pFile);
		}

		m_bFirstWrite = false;
	}

	Utf8_Iter iter8;
	iter8.set(static_cast<const ubyte *>(p), _size, m_eEncoding);

	utf16 *pCur = m_buf16.get();

	for (; iter8; ++iter8) {
		if (iter8.canGet()) {
			int codePoint = iter8.get();
			if (codePoint >= SURROGATE_FIRST_VALUE) {
				codePoint -= SURROGATE_FIRST_VALUE;
				const int lead = (codePoint >> 10) + SURROGATE_LEAD_FIRST;
				*pCur++ = static_cast<utf16>((m_eEncoding == eUtf16BigEndian) ?
							     swapped(lead) : lead);
				const int trail = (codePoint & 0x3ff) + SURROGATE_TRAIL_FIRST;
				*pCur++ = static_cast<utf16>((m_eEncoding == eUtf16BigEndian) ?
							     swapped(trail) : trail);
			} else {
				*pCur++ = static_cast<utf16>((m_eEncoding == eUtf16BigEndian) ?
							     swapped(codePoint) : codePoint);
			}
		}
	}

	const size_t ret = ::fwrite(m_buf16.get(),
				    reinterpret_cast<const char *>(pCur) - reinterpret_cast<const char *>(m_buf16.get()),
				    1, m_pFile);

	return ret;
}

int Utf8_16_Write::fclose() noexcept {
	m_buf16.reset();

	const int ret = ::fclose(m_pFile);
	m_pFile = nullptr;

	return ret;
}

void Utf8_16_Write::setEncoding(Utf8_16::encodingType eType) noexcept {
	m_eEncoding = eType;
}

//=================================================================
Utf8_Iter::Utf8_Iter() noexcept {
	m_pBuf = nullptr;
	m_pRead = nullptr;
	m_pEnd = nullptr;
	m_eState = eStart;
	m_nCur = 0;
	m_eEncoding = eUnknown;
}

void Utf8_Iter::set
(const ubyte *pBuf, size_t nLen, encodingType eEncoding) noexcept {
	m_pBuf = pBuf;
	m_pRead = pBuf;
	m_pEnd = pBuf + nLen;
	m_eEncoding = eEncoding;
	operator++();
	// Note: m_eState, m_nCur not reset
}
// Go to the next byte.
void Utf8_Iter::operator++() noexcept {
	switch (m_eState) {
	case eStart:
		if ((0xF0 & *m_pRead) == 0xF0) {
			m_nCur = (0x7 & *m_pRead) << 18;
			m_eState = eSecondOf4Bytes;
		} else if ((0xE0 & *m_pRead) == 0xE0) {
			m_nCur = (~0xE0 & *m_pRead) << 12;
			m_eState = ePenultimate;
		} else if ((0xC0 & *m_pRead) == 0xC0) {
			m_nCur = (~0xC0 & *m_pRead) << 6;
			m_eState = eFinal;
		} else {
			m_nCur = *m_pRead;
			toStart();
		}
		break;
	case eSecondOf4Bytes:
		m_nCur |= (0x3F & *m_pRead) << 12;
		m_eState = ePenultimate;
		break;
	case ePenultimate:
		m_nCur |= (0x3F & *m_pRead) << 6;
		m_eState = eFinal;
		break;
	case eFinal:
		m_nCur |= static_cast<utf8>(0x3F & *m_pRead);
		toStart();
		break;
	}
	++m_pRead;
}

void Utf8_Iter::toStart() noexcept {
	m_eState = eStart;
}

//==================================================
Utf16_Iter::Utf16_Iter() noexcept {
	m_pBuf = nullptr;
	m_pRead = nullptr;
	m_pEnd = nullptr;
	m_eState = eStart;
	m_nCur = 0;
	m_nCur16 = 0;
	m_eEncoding = eUnknown;
}

void Utf16_Iter::set
(const ubyte *pBuf, size_t nLen, encodingType eEncoding, ubyte *endSurrogate) noexcept {
	m_pBuf = pBuf;
	m_pRead = pBuf;
	m_pEnd = pBuf + nLen;
	m_eEncoding = eEncoding;
	if (nLen > 2) {
		const utf16 lastElement = read(m_pEnd-2);
		if (lastElement >= SURROGATE_LEAD_FIRST && lastElement <= SURROGATE_LEAD_LAST) {
			// Buffer ends with lead surrogate so cut off buffer and store
			endSurrogate[0] = m_pEnd[-2];
			endSurrogate[1] = m_pEnd[-1];
			m_pEnd -= 2;
		}
	}
	operator++();
	// Note: m_eState, m_nCur, m_nCur16 not reinitialized.
}

// Goes to the next byte.
// Not the next symbol which you might expect.
// This way we can continue from a partial buffer that doesn't align
void Utf16_Iter::operator++() noexcept {
	switch (m_eState) {
	case eStart:
		if (m_pRead >= m_pEnd) {
			++m_pRead;
			break;
		}
		if (m_eEncoding == eUtf16LittleEndian) {
			m_nCur16 = *m_pRead++;
			m_nCur16 |= static_cast<utf16>(*m_pRead << 8);
		} else {
			m_nCur16 = static_cast<utf16>(*m_pRead++ << 8);
			m_nCur16 |= *m_pRead;
		}
		if (m_nCur16 >= SURROGATE_LEAD_FIRST && m_nCur16 <= SURROGATE_LEAD_LAST) {
			++m_pRead;
			if (m_pRead >= m_pEnd) {
				// Have a lead surrogate at end of document with no access to trail surrogate.
				// May be end of document.
				--m_pRead;	// With next increment, leave pointer just past buffer
			} else {
				int trail;
				if (m_eEncoding == eUtf16LittleEndian) {
					trail = *m_pRead++;
					trail |= static_cast<utf16>(*m_pRead << 8);
				} else {
					trail = static_cast<utf16>(*m_pRead++ << 8);
					trail |= *m_pRead;
				}
				m_nCur16 = (((m_nCur16 & 0x3ff) << 10) | (trail & 0x3ff)) + SURROGATE_FIRST_VALUE;
			}
		}
		++m_pRead;

		if (m_nCur16 < 0x80) {
			m_nCur = static_cast<ubyte>(m_nCur16 & 0xFF);
			m_eState = eStart;
		} else if (m_nCur16 < 0x800) {
			m_nCur = static_cast<ubyte>(0xC0 | m_nCur16 >> 6);
			m_eState = eFinal;
		} else if (m_nCur16 < SURROGATE_FIRST_VALUE) {
			m_nCur = static_cast<ubyte>(0xE0 | m_nCur16 >> 12);
			m_eState = ePenultimate;
		} else {
			m_nCur = static_cast<ubyte>(0xF0 | m_nCur16 >> 18);
			m_eState = eSecondOf4Bytes;
		}
		break;
	case eSecondOf4Bytes:
		m_nCur = static_cast<ubyte>(0x80 | ((m_nCur16 >> 12) & 0x3F));
		m_eState = ePenultimate;
		break;
	case ePenultimate:
		m_nCur = static_cast<ubyte>(0x80 | ((m_nCur16 >> 6) & 0x3F));
		m_eState = eFinal;
		break;
	case eFinal:
		m_nCur = static_cast<ubyte>(0x80 | (m_nCur16 & 0x3F));
		m_eState = eStart;
		break;
	}
}

Utf8_16::utf16 Utf16_Iter::read(const ubyte *pRead) const noexcept {
	if (m_eEncoding == eUtf16LittleEndian) {
		return pRead[0] | static_cast<utf16>(pRead[1] << 8);
	} else {
		return pRead[1] | static_cast<utf16>(pRead[0] << 8);
	}
}