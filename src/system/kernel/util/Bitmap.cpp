/*
 * Copyright 2013-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 *		Augustin Cavalier <waddlesplash>
 */
#include <util/Bitmap.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <util/BitUtils.h>


namespace BKernel {


Bitmap::Bitmap(size_t bitCount)
	:
	fElementsCount(0),
	fSize(0),
	fBits(NULL)
{
	Resize(bitCount);
}


Bitmap::~Bitmap()
{
	free(fBits);
}


status_t
Bitmap::InitCheck()
{
	return (fBits != NULL) ? B_OK : B_NO_MEMORY;
}


status_t
Bitmap::Resize(size_t bitCount)
{
	if (bitCount > SIZE_MAX - kBitsPerElement)
		return B_NO_MEMORY;

	const size_t count = (bitCount + kBitsPerElement - 1) / kBitsPerElement;
	if (count == fElementsCount) {
		fSize = bitCount;
		return B_OK;
	}

	if (count > SIZE_MAX / sizeof(addr_t))
		return B_NO_MEMORY;

	void* bits = realloc(fBits, sizeof(addr_t) * count);
	if (bits == NULL)
		return B_NO_MEMORY;
	fBits = (addr_t*)bits;

	if (fElementsCount < count)
		memset(&fBits[fElementsCount], 0, sizeof(addr_t) * (count - fElementsCount));

	fSize = bitCount;
	fElementsCount = count;
	return B_OK;
}


void
Bitmap::Shift(ssize_t bitCount)
{
	return bitmap_shift<addr_t>(fBits, fSize, bitCount);
}


void
Bitmap::SetRange(size_t index, size_t count)
{
	if (count == 0)
		return;

	ASSERT(index + count <= fSize);

	size_t startWord = index / kBitsPerElement;
	size_t endWord = (index + count) / kBitsPerElement;
	size_t startBit = index % kBitsPerElement;
	size_t endBit = (index + count) % kBitsPerElement;

	if (startWord == endWord) {
		addr_t mask;
		if (count == kBitsPerElement)
			mask = ~(addr_t)0;
		else
			mask = ((addr_t(1) << count) - 1) << startBit;
		fBits[startWord] |= mask;
	} else {
		if (startBit > 0) {
			fBits[startWord++] |= ~(addr_t(0)) << startBit;
		}

		while (startWord < endWord) {
			fBits[startWord++] = ~(addr_t(0));
		}

		if (endBit > 0) {
			fBits[endWord] |= (addr_t(1) << endBit) - 1;
		}
	}
}


void
Bitmap::ClearRange(size_t index, size_t count)
{
	if (count == 0)
		return;

	ASSERT(index + count <= fSize);

	size_t startWord = index / kBitsPerElement;
	size_t endWord = (index + count) / kBitsPerElement;
	size_t startBit = index % kBitsPerElement;
	size_t endBit = (index + count) % kBitsPerElement;

	if (startWord == endWord) {
		addr_t mask;
		if (count == kBitsPerElement)
			mask = ~(addr_t)0;
		else
			mask = ((addr_t(1) << count) - 1) << startBit;
		fBits[startWord] &= ~mask;
	} else {
		if (startBit > 0) {
			fBits[startWord++] &= ~(~(addr_t(0)) << startBit);
		}

		while (startWord < endWord) {
			fBits[startWord++] = 0;
		}

		if (endBit > 0) {
			fBits[endWord] &= ~((addr_t(1) << endBit) - 1);
		}
	}
}


ssize_t
Bitmap::GetLowestClear(size_t fromIndex) const
{
	if (fromIndex >= fSize)
		return -1;

	size_t startWord = fromIndex / kBitsPerElement;
	size_t startBit = fromIndex % kBitsPerElement;

	// Check the first word specially
	addr_t word = fBits[startWord];
	if (startBit > 0)
		word |= (addr_t(1) << startBit) - 1;

	if (word != ~(addr_t(0))) {
		for (size_t k = startBit; k < kBitsPerElement; k++) {
			if (!((word >> k) & 1)) {
				size_t result = startWord * kBitsPerElement + k;
				if (result >= fSize)
					return -1;
				return result;
			}
		}
	}

	size_t endWord = (fSize + kBitsPerElement - 1) / kBitsPerElement;
	for (size_t i = startWord + 1; i < endWord; i++) {
		word = fBits[i];
		if (word != ~(addr_t(0))) {
			for (size_t k = 0; k < kBitsPerElement; k++) {
				if (!((word >> k) & 1)) {
					size_t result = i * kBitsPerElement + k;
					if (result >= fSize)
						return -1;
					return result;
				}
			}
		}
	}

	return -1;
}


ssize_t
Bitmap::GetLowestContiguousClear(size_t count, size_t fromIndex) const
{
	if (count == 0)
		return fromIndex;
	if (fromIndex >= fSize)
		return -1;

	for (;;) {
		ssize_t index = GetLowestClear(fromIndex);
		if (index < 0)
			return -1;

		if (count > fSize || (size_t)index > fSize - count)
			return -1;

		// Check if [index + 1, index + count) is clear
		bool allClear = true;
		size_t i = 1;

		// Align to word boundary
		while (i < count && (index + i) % kBitsPerElement != 0) {
			if (Get(index + i)) {
				allClear = false;
				break;
			}
			i++;
		}

		if (allClear) {
			// Check full words
			while (i + kBitsPerElement <= count) {
				size_t wordIdx = (index + i) / kBitsPerElement;
				addr_t word = fBits[wordIdx];
				if (word != 0) {
					allClear = false;
					// Find the first set bit to skip properly
					for (size_t k = 0; k < kBitsPerElement; k++) {
						if ((word >> k) & 1) {
							i += k;
							break;
						}
					}
					break;
				}
				i += kBitsPerElement;
			}
		}

		if (allClear) {
			// Check remaining bits
			while (i < count) {
				if (Get(index + i)) {
					allClear = false;
					break;
				}
				i++;
			}
		}

		if (allClear)
			return index;

		fromIndex = index + i + 1;
	}
}


ssize_t
Bitmap::GetHighestSet() const
{
	ssize_t index = -1;
	for (size_t i = fElementsCount; i > 0; i--) {
		if (fBits[i - 1] != 0) {
			index = (ssize_t)(i - 1);
			break;
		}
	}

	if (index < 0)
		return -1;

	STATIC_ASSERT(sizeof(addr_t) == sizeof(uint64)
		|| sizeof(addr_t) == sizeof(uint32));
	if (sizeof(addr_t) == sizeof(uint32))
		return log2(fBits[index]) + index * kBitsPerElement;

	uint32 v = (uint64)fBits[index] >> 32;
	if (v != 0)
		return log2(v) + sizeof(uint32) * 8 + index * kBitsPerElement;
	return log2(fBits[index]) + index * kBitsPerElement;
}


} // namespace BKernel
