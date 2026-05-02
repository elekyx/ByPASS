#include "Data_Cache_Flash.h"
#include <assert.h>


namespace SSD_Components
{
	Data_Cache_Flash::Data_Cache_Flash(unsigned int capacity_in_pages) : capacity_in_pages(capacity_in_pages) {}

	bool Data_Cache_Flash::Exists(const stream_id_type stream_id, const LPA_type lpn)
	{
		return Find_slot(stream_id, lpn) != nullptr;
	}

	

	Data_Cache_Slot_Type* Data_Cache_Flash::Find_slot(const stream_id_type stream_id, const LPA_type lpn)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		if (it == slots.end()) {
			return nullptr;
		}
		return it->second;
	}

	void Data_Cache_Flash::Move_to_front(Data_Cache_Slot_Type* slot)
	{
		assert(slot != nullptr);
		if (lru_list.begin()->second != slot) {
			lru_list.splice(lru_list.begin(), lru_list, slot->lru_list_ptr);
		}
	}

	


	Data_Cache_Flash::~Data_Cache_Flash()
	{
		for (auto &slot : slots) {
			delete slot.second;
		}
	}

	Data_Cache_Slot_Type Data_Cache_Flash::Get_slot(const stream_id_type stream_id, const LPA_type lpn)
	{
		Data_Cache_Slot_Type* slot = Find_slot(stream_id, lpn);
		assert(slot != nullptr);
		Move_to_front(slot);
		return *slot;
	}

	void Data_Cache_Flash::Mark_slot_accessed(Data_Cache_Slot_Type* slot)
	{
		assert(slot != nullptr);
		slot->ever_hit = true;
		Move_to_front(slot);
	}

	void Data_Cache_Flash::Mark_slot_accessed(const stream_id_type stream_id, const LPA_type lpn)
	{
		Data_Cache_Slot_Type* slot = Find_slot(stream_id, lpn);
		assert(slot != nullptr);
		Mark_slot_accessed(slot);
	}

	bool Data_Cache_Flash::Check_free_slot_availability()
	{
		return slots.size() < capacity_in_pages;
	}

	bool Data_Cache_Flash::Check_free_slot_availability(unsigned int no_of_slots)
	{
		return slots.size() + no_of_slots <= capacity_in_pages;
	}

	bool Data_Cache_Flash::Empty()
	{
		return slots.size() == 0;
	}

	bool Data_Cache_Flash::Full()
	{
		
		return slots.size() >= capacity_in_pages;
	}

	

	Data_Cache_Slot_Type Data_Cache_Flash::Evict_one_dirty_slot()
	{
		assert(slots.size() > 0);
		auto itr = lru_list.rbegin();
		while (itr != lru_list.rend()) {
			if ((*itr).second->Status == Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK) {
				break;
			}
			itr++;
		}

		Data_Cache_Slot_Type evicted_item = *lru_list.back().second;
		if (itr == lru_list.rend()) {
			evicted_item.Status = Cache_Slot_Status::EMPTY;
			return evicted_item;
		}

		slots.erase(lru_list.back().first);
		delete lru_list.back().second;
		lru_list.pop_back();
		
		return evicted_item;
	}

	Data_Cache_Slot_Type Data_Cache_Flash::Evict_one_slot(LPA_type key){
		auto iter = lru_list.begin();
		while(iter != lru_list.end()){
			if((*iter).first == key){
				break;
			}
			iter++;
		}
		Data_Cache_Slot_Type evicted_item = *(slots[key]);
		delete slots[key];
		slots.erase(key);
		lru_list.erase(iter);
		return evicted_item;
	}

	Data_Cache_Slot_Type Data_Cache_Flash::Evict_one_slot_lru()
	{
		assert(slots.size() > 0);
		slots.erase(lru_list.back().first);
		Data_Cache_Slot_Type evicted_item = *lru_list.back().second;
		delete lru_list.back().second;
		lru_list.pop_back();

		return evicted_item;
	}

	void Data_Cache_Flash::Change_slot_status_to_writeback(const stream_id_type stream_id, const LPA_type lpn)
	{
		Data_Cache_Slot_Type* slot = Find_slot(stream_id, lpn);
		assert(slot != nullptr);
		slot->Status = Cache_Slot_Status::DIRTY_FLASH_WRITEBACK;
	}

	void Data_Cache_Flash::Insert_read_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_read_sectors)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		
		if (slots.find(key) != slots.end()) {
			throw std::logic_error("Duplicate lpn insertion into data cache!");
		}
		if (slots.size() >= capacity_in_pages) {
			throw std::logic_error("Data cache overfull!");
		}

		Data_Cache_Slot_Type* cache_slot = new Data_Cache_Slot_Type();
		cache_slot->ever_hit = false;
		cache_slot->LPA = lpn;
		cache_slot->streamid = stream_id;
		cache_slot->State_bitmap_of_existing_sectors = state_bitmap_of_read_sectors;
		cache_slot->Content = content;
		cache_slot->Timestamp = timestamp;
		cache_slot->Status = Cache_Slot_Status::CLEAN;
		lru_list.push_front(std::pair<LPA_type, Data_Cache_Slot_Type*>(key, cache_slot));
		cache_slot->lru_list_ptr = lru_list.begin();
		slots[key] = cache_slot;
	}

	void Data_Cache_Flash::Insert_write_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		
		if (slots.find(key) != slots.end()) {
			throw std::logic_error("Duplicate lpn insertion into data cache!!");
		}
		
		if (slots.size() >= capacity_in_pages) {
			throw std::logic_error("Data cache overfull!");
		}

		Data_Cache_Slot_Type* cache_slot = new Data_Cache_Slot_Type();
		cache_slot->ever_hit = false;
		cache_slot->LPA = lpn;
		cache_slot->streamid = stream_id;
		cache_slot->State_bitmap_of_existing_sectors = state_bitmap_of_write_sectors;
		cache_slot->Content = content;
		cache_slot->Timestamp = timestamp;
		cache_slot->Status = Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK;
		lru_list.push_front(std::pair<LPA_type, Data_Cache_Slot_Type*>(key, cache_slot));
		cache_slot->lru_list_ptr = lru_list.begin();
		slots[key] = cache_slot;
	}

	void Data_Cache_Flash::Update_data(Data_Cache_Slot_Type* slot, const stream_id_type stream_id, const LPA_type lpn,
		const data_cache_content_type content, const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		assert(slot != nullptr);
		slot->ever_hit = true;
		slot->streamid = stream_id;
		slot->LPA = lpn;
		slot->State_bitmap_of_existing_sectors = state_bitmap_of_write_sectors;
		slot->Content = content;
		slot->Timestamp = timestamp;
		slot->Status = Cache_Slot_Status::DIRTY_NO_FLASH_WRITEBACK;
		Move_to_front(slot);
	}

	void Data_Cache_Flash::Update_data(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		Data_Cache_Slot_Type* slot = Find_slot(stream_id, lpn);
		assert(slot != nullptr);
		Update_data(slot, stream_id, lpn, content, timestamp, state_bitmap_of_write_sectors);
	}

	void Data_Cache_Flash::Update_data_read(Data_Cache_Slot_Type* slot, const stream_id_type stream_id, const LPA_type lpn,
		const data_cache_content_type content, const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		assert(slot != nullptr);
		slot->ever_hit = true;
		slot->streamid = stream_id;
		slot->LPA = lpn;
		slot->State_bitmap_of_existing_sectors = state_bitmap_of_write_sectors;
		slot->Content = content;
		slot->Timestamp = timestamp;
		Move_to_front(slot);
	}

	void Data_Cache_Flash::Update_data_read(const stream_id_type stream_id, const LPA_type lpn, const data_cache_content_type content,
		const data_timestamp_type timestamp, const page_status_type state_bitmap_of_write_sectors)
	{
		Data_Cache_Slot_Type* slot = Find_slot(stream_id, lpn);
		assert(slot != nullptr);
		Update_data_read(slot, stream_id, lpn, content, timestamp, state_bitmap_of_write_sectors);
	}

	unsigned int Data_Cache_Flash::get_capacity(){
		return capacity_in_pages;
	}
	
	void Data_Cache_Flash::set_capacity(unsigned int capacity){
		capacity_in_pages = capacity;
	}

	void Data_Cache_Flash::Remove_slot(const stream_id_type stream_id, const LPA_type lpn)
	{
		LPA_type key = LPN_TO_UNIQUE_KEY(stream_id, lpn);
		auto it = slots.find(key);
		assert(it != slots.end());
		lru_list.erase(it->second->lru_list_ptr);
		delete it->second;
		slots.erase(it);
	}
}
