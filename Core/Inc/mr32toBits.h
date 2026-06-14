/*
 * mr32toBits.h
 *
 *  Created on: Aug 12, 2025
 *      Author: michael
 */

#ifndef MR32_TO_BITS_H
#define MR32_TO_BITS_H

#include <stdint.h>
#include <stddef.h>
#include <string>

// Basis-Funktion für Rohdaten
std::string mr32toBits(const void *value, size_t size, int8_t delimit = 0);

// Template-Funktion für beliebige Datentypen
template<typename T>
std::string mr32toBits(T value, int8_t delimit = 0) {
	return mr32toBits(&value, sizeof(T), delimit);
}

#endif // MR32_TO_BITS_H
