/*
 * Copyright (c) 2021 George Beckstein
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#ifndef _RINGFSBLOCKDEVICE_H_
#define _RINGFSBLOCKDEVICE_H_

#include "BlockDevice.h"
#include "ringfs.h"

	/**
	 * C++ library based on the RingFS library by Kosma Moczek
	 *
	 * RingFS is a small, flash-based ring buffer for embedded software.
	 * This helps with wear leveling of flash chips used for data caching
	 *
	 * See the README for more information
	 *
	 * This class operates on a data structure of the type
	 * that you instantiate it with in the template
	 *
	 * Ringfs databases with different data structures are incompatible!
	 *
	 */
	template <class T>
	class Ringfs
	{
		public:

			/**
			 * Instantiate a Ringfs file system
			 *
			 * @note The database version must be unique among Ringfs systems
			 * that use different datatypes. Otherwise may result in undefined behavior
			 *
			 * @param[in] blockdevice Block device to back the RingfsBlockDevice
			 * @param[in] database_uuid Unique identification for database version
			 */
			Ringfs<T>(BlockDevice* blockdevice, uint32_t database_uuid)
			: _block_device(blockdevice)
			{
				// Initialize C structures
				_flash.sector_size 		= _block_device->get_erase_size();
				_flash.sector_offset 	= 0;
				_flash.sector_count 		= (_block_device->size() / _flash.sector_size);
				_flash.sector_erase 		= mbed::callback(this, &Ringfs<T>::_sector_erase);
				_flash.program 			= mbed::callback(this, &Ringfs<T>::_program);
				_flash.read 				= mbed::callback(this, &Ringfs<T>::_read);

				ringfs_init(&_ringfs, &_flash,
						database_uuid, sizeof(T));
			}

			/**
			 * Format the block device into a Ringfs file system
			 *
			 * @note this effectively removes all data currently stored in flash!
			 *
			 *	@retval error 0 on success, -1 on failure
			 */
			int format(void)
			{
				return ringfs_format(&_ringfs);
			}

			/**
			 * Scan the block device for a valid Ringfs file system
			 *
			 * @note any existing Ringfs file system must use the exact same
			 * template type/data structure to be considered valid!
			 *
			 *	@retval error 0 on success, -1 on failure
			 */
			int scan(void)
			{
				return ringfs_scan(&_ringfs);
			}

			/**
			 * Calculates the maximum capacity of this Ringfs file system
			 *
			 * @retval capacity maximum capacity on success, -1 on failure
			 */
			int maximum_capacity(void)
			{
				return ringfs_capacity(&_ringfs);
			}

			/**
			 * Calculate approximate object count
			 * Runs in O(1)
			 *
			 * @retval estimate Estimated object count on success, -1 on failure
			 */
			int estimate_number_of_files(void)
			{
				return ringfs_count_estimate(&_ringfs);
			}

			/**
			 * Calculate exact object count.
			 * Runs in O(n).
			 *
			 * @retval exact Exact object count on success, -1 on failure
			 */
			int exact_number_of_files(void)
			{
				return ringfs_count_exact(&_ringfs);
			}

			/**
			 * Append an object at the end of the ring. Deletes oldest objects as needed.
			 *
			 * @param[in] object Object pointer to append
			 *
			 * @retval error 0 on success, -1 on failure
			 */
			int append(T* object)
			{
				return ringfs_append(&_ringfs, object);
			}

			/**
			 * Fetch next object from the ring, oldest-first. Advances read cursor.
			 *
			 * @param[out] object Buffer to hold returns object
			 *
			 * @retval error 0 on success, -1 on failure
			 */
			int fetch(T* object)
			{
				return ringfs_fetch(&_ringfs, object);
			}

			/**
			 * Discard all fetched objects up to the read cursor.
			 *
			 * @retval error 0 on success, -1 on failure
			 */
			int discard(void)
			{
				return ringfs_discard(&_ringfs);
			}

			/**
			 * Rewind the read cursor back to the oldest object
			 *
			 * @retval error 0 on success, -1 on failure
			 */
			int rewind(void)
			{
				return ringfs_rewind(&_ringfs);
			}

			/**
			 * Dump filesystem metadata. For debugging purposes.
			 * @param stream File stream to write to
			 */
			void dump(FILE* stream)
			{
				ringfs_dump(stream, &_ringfs);
			}

		protected:

			/**
			 * Internal C api implementation
			 * Erase a sector.
			 * @param address Any address inside the sector.
			 * @returns Zero on success, -1 on failure.
			 */
			int _sector_erase(ringfs_flash_partition_t* flash,
					int address)
			{
				if(_block_device->erase(address,
						_block_device->get_erase_size()) != 0)
					return -1;
				else
					return 0;
			}

			/**
 			 * Internal C api implementation
			 * Program flash memory bits by toggling them from 1 to 0.
			 * @param address Start address, in bytes.
			 * @param data Data to program.
			 * @param size Size of data.
			 * @returns size on success, -1 on failure.
			 */
			 ssize_t _program(ringfs_flash_partition_t* flash,
					 int address, const void* data, size_t size)
			 {
				 if(_block_device->program(data, address, size) != 0)
					return -1;
				else
					return size;
			 }

			 /**
			  * Internal C api implementation
			  * Read flash memory.
			  * @param address Start address, in bytes.
			  * @param data Buffer to store read data.
			  * @param size Size of data.
			  * @returns size on success, -1 on failure.
			  */
			 ssize_t _read(ringfs_flash_partition_t* flash,
					 int address, void* data, size_t size)
			 {
				 if(_block_device->read(data, address, size) != 0)
					return -1;
				else
					return size;
			 }

		protected:

			/** Underlying block device for Ringfs file system */
			BlockDevice* _block_device;

			/** Underlying ringfs C structure */
			struct ringfs _ringfs;

			/** Underlying ringfs flash partition C structure */
			struct ringfs_flash_partition _flash;

	};

#endif /* _RINGFSBLOCKDEVICE_H_ */
