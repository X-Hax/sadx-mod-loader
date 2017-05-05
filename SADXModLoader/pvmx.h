#pragma once

#include <cstdint>
#include <fstream>
#include <vector>
#include "TextureReplacement.h"

constexpr uint32_t PVMX_FOURCC  = 'XMVP';
constexpr uint8_t  PVMX_VERSION = 1;

namespace pvmx
{
	struct DictionaryEntry : TexPackEntry
	{
		uint64_t offset;
		uint64_t size;
	};

	namespace DictionaryField
	{
		enum _ : uint8_t
		{
			None,
			// 32-bit integer global index
			GlobalIndex,
			// Null-terminated file name
			Name,
			// Two 32-bit integers defining width and height
			Dimensions,
		};

		static_assert(sizeof(None) == sizeof(uint8_t), "DictionaryField size mismatch");
	}

	/**
	 * \brief Checks the header of the provided file and restores the stream position.
	 * \param file The file to check.
	 * \return true if the file is a PVMX archive.
	 */
	bool is_pvmx(std::ifstream& file);

	bool is_pvmx(const char* path);
	bool is_pvmx(const std::string& path);

	/**
	* \brief Same as is_pvmx, but does not restore the stream position.
	* \param file The file to check.
	* \return true if the file is a PVMX archive.
	*/
	bool check_header(std::ifstream& file);

	/**
	 * \brief Reads the texture pack index from the given PVMX archive.
	 * \param file The file to read from.
	 * \param out A vector to be populated with the texture pack index.
	 * \return true on success.
	 */
	bool read_index(std::ifstream& file, std::vector<DictionaryEntry>& out);

	/**
	 * \brief Reads a texture entry into the provided buffer.
	 * \param file The file to read from.
	 * \param entry The entry whose data is to be read.
	 * \param out The buffer to store the data.
	 * \return true on success.
	 */
	bool get_entry(std::ifstream& file, const DictionaryEntry& entry, std::vector<uint8_t>& out);
}
