/*
 * memory.hpp - ncr memory management functions and utilities
 *
 * SPDX-FileCopyrightText: 2024 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 * TODO: provide some logging capabilities in slab_memory
 */
#pragma once

#include <cstddef>
#include <vector>
#include <list>
#include <optional>
#include <ostream>


namespace ncr {


/*
 * memory_guard - A simple memory scope guard
 *
 * This template allows to pass in several pointers which will be deleted when
 * the guard goes out of scope. This is the same as using a unique_ptr into
 * which a pointer is moved, but with slightly less verbose syntax. This is
 * particularly useful to scope members of structs / classes.
 *
 * Example:
 *
 *     struct MyStruct {
 *         SomeType *var;
 *
 *         void some_function
 *         {
 *             // for some reason, var should live only as long as some_function
 *             // is running. This can be useful in the case of recursive
 *             // functions to which sending all the context or state variables
 *             // is inconvenient, and save them in the surrounding struct. An
 *             // alternative, maybe even a preferred way, is to use PODs that
 *             // contain state and use free functions. Still, the memory guard
 *             // might be handy
 *             var = new SomeType{};
 *             memory_guard<SomeType> guard(var);
 *
 *             ...
 *
 *             // the guard will call delete on `var' once it drops out of scope
 *         }
 *     };
 *
 * Example with unique_ptr:
 *
 *            // .. struct is same as above
 *            var = new SomeType{};
 *            std::unique_ptr<SomeType>(std::move(*var));
 *
 * Yes, this only saves a few characters to type. However, memory_guard works
 * with an arbitrary number of arguments.
 */
template <typename... Ts>
struct memory_guard;

template <>
struct memory_guard<> {};

template <typename T, typename... Ts>
struct memory_guard<T, Ts...> : memory_guard<Ts...>
{
	T *ptr = nullptr;
	memory_guard(T *_ptr, Ts *...ptrs) : memory_guard<Ts...>(ptrs...), ptr(_ptr) {}
	~memory_guard() { if (ptr) delete ptr; }
};


/*
 * Identifiers used throughout the slab memory are of type slab_memory_index_t. Here,
 * they are simply ptrdiff_t to be able to identify invalid memory IDs. Note
 * that the slab memory does mostly not care about the exact type that is
 * contained within the memory, and operates mostly on slab_memory_indexs.
 */
using slab_memory_index_t = std::size_t;


/*
 * slab_memory_item - memory item for arbitrary types T
 *
 * TODO: separate reference counting from data into two independent structs
 */
template <typename T>
struct slab_memory_item {
	// id
	std::optional<slab_memory_index_t> index = {};

	// reference count of this memory
	size_t ref_count = 0;

	// value of the memory
	T value;
};


// default values for the slab memory
static constexpr const size_t slab_memory_default_page_size = 2048;


/*
 * some statistics that we might collect over the span of the slab_memory
 * lifetime
 */
struct slab_memory_stats {
	size_t capacity         = 0;
	size_t size             = 0;
	size_t page_count       = 0;
	size_t page_size        = slab_memory_default_page_size;
	size_t real_allocated   = 0;
	size_t real_released    = 0;
	size_t total_allocated  = 0;
	size_t total_freed      = 0;
	size_t invalid_released = 0;
	size_t invalid_freed    = 0;
	size_t total_reused     = 0;
	size_t invalid_decref   = 0;
	size_t invalid_incref   = 0;
	size_t total_incref     = 0;
	size_t total_decref     = 0;
};


/*
 * poor man's memory management with a slab memory system that internally uses
 * pages to store memory in order to reduce memory allocations.
 *
 * This is effectively an arena or pool allocator that allocates bigger slabs of
 * memory to store things.
 *
 * Paging is used primarily to avoid invalidating pointers to items. That is,
 * the individual pages will not be moved around after allocation. However, the
 * vector with the pointers to pages might be moved.
 */
template <typename T>
struct slab_memory
{
	// types used within this memory
	using index_type        = slab_memory_index_t;
	using item_type	        = slab_memory_item<T>;
	using memory_type       = std::vector<item_type>;
	using page_type         = std::vector<memory_type*>;
	template <typename ValueType> using optional = std::optional<ValueType>;

	// functions
	optional<index_type>	alloc();
	optional<size_t>        free(optional<index_type> index);
	optional<index_type>	copy(const optional<index_type> index);
	optional<size_t>        incref(const optional<index_type> index);
	optional<size_t>        decref(const optional<index_type> index);

	std::vector<optional<index_type>> calloc(size_t N);
	// TODO: free version for vector

	// TODO: needs iterators .begin(), .end(), ++, etc. to walk over all memory
	//       items

	optional<T * const>     get(const optional<index_type> index);
	void                    set(const optional<index_type> index, T&& value);

	size_t                  capacity()   const { return this->_stats.capacity; };
	size_t                  size()       const { return this->_stats.size; };
	size_t                  page_count() const { return this->_stats.page_count; };
	size_t                  page_size()  const { return this->_stats.page_size; };
	slab_memory_stats	stats()      const { return this->_stats; };

	// constructor, destructor, for pre-allocating and cleaning up memory
	slab_memory(const size_t page_size = slab_memory_default_page_size);
	~slab_memory();

private:
	void                    _release(const optional<index_type> index);
	item_type&              _get_item(size_t index);
	void                    _alloc_page();

	// the memory itself
	page_type               pages;
	std::list<index_type>	_free_indexes;
	size_t                  _last_index = 0;

	// statistics and maintenance
	slab_memory_stats       _stats;
};


/*
 * slab_memory::_alloc_page - allocate a new page
 */
template <typename T>
void
slab_memory<T>::_alloc_page()
{
	auto page = new memory_type();
	page->resize(this->_stats.page_size);
	this->pages.push_back(page);

	// update stats and maintenance
	this->_stats.page_count += 1;
	this->_stats.capacity += this->_stats.page_size;
}


/*
 * slab_memory::slab_memory - initialize a new slab_memory
 */
template <typename T>
slab_memory<T>::slab_memory(const size_t page_size)
{
	// initialize necessary defaults
	_stats.page_count = 0;
	_stats.page_size = page_size > 0 ? page_size : slab_memory_default_page_size;

	// allocate the first page
	this->_alloc_page();
}


/*
 * slab_memory::~slab_memory - release a slab_memory, freeing up memory
 */
template <typename T>
slab_memory<T>::~slab_memory()
{
	for (size_t i = 0; i < this->pages.size(); ++i) {
		delete this->pages[i];
		this->pages[i] = nullptr;
	}
}


/*
 * slab_memory::_get_item - get the item for a given index
 */
template <typename T>
auto
slab_memory<T>::_get_item(size_t index) -> item_type&
{
	const size_t page_index  = index / this->_stats.page_size;
	const size_t page_offset = index % this->_stats.page_size;
	memory_type& page  = *this->pages[page_index];
	item_type& item    = page[page_offset];
	return item;
}

/*
 * slab_memory::get - get const pointer to value of certain memory location
 *
 * This returns a const pointer to the memory. There might be other consumers
 * of the memory, so avoid changing it. Only the owner (i.e. creator) of the
 * resource should change it. If you're absolutely sure that you received a copy
 * of original data, and you're the only consumer, of course you can cast away
 * the constness.
 */
template <typename T>
auto
slab_memory<T>::get(const optional<slab_memory_index_t> index) -> optional<T * const>
{
	if (!index)
		return {};

	item_type &mem_item = this->_get_item(index.value());
	std::optional<T*> result = &mem_item.value;
	if (mem_item.ref_count <= 0) {
		//log_warning("slab_memory::get_ptr() on memory with refcount <= 0.");
		// TODO: this should probably return an empty, and not the
		// negative-refcount element in the memory
	}
	return result;
}


/*
 * slab_memory::set - set value of a certain memory location
 */
template <typename T>
void
slab_memory<T>::set(const optional<slab_memory_index_t> index, T&& value)
{
	if (!index)
		return;

	item_type& item = this->_get_item(index.value());
	item.value = std::move(value);
}


/*
 * slab_memory::alloc - Get a pointer to a new memory item
 *
 * Returns a pointer to a slab_memory_item. The memory used for the memory can be
 * either newly allocated, or re-used memory of a prior memory whose reference
 * count dropped to 0.
 */
template <typename T>
auto
slab_memory<T>::alloc() -> optional<index_type>
{
	//log_verbose("slab_memory::alloc()\n");
	std::optional<index_type> index{};

	// re-use memory that was freed in a previous run, or use last slab space element?
	if (this->_free_indexes.size() > 0) {
		// grab the first ID in the list
		index = this->_free_indexes.front();
		this->_free_indexes.pop_front();
		this->_stats.total_reused += 1;

		//log_verbose("    repurposed id = ", index.value(), "\n");
	}
	else {
		// allocate new memory if needed
		if (this->_last_index >= this->capacity())
			this->_alloc_page();

		index = this->_last_index++;
		this->_stats.real_allocated += 1;
		//log_verbose("    new id      = ", index.value(), "\n");
	}
	//log_verbose( "    memory size = ", this->capacity(), "\n");

	// ref-counting and statistics
	item_type &item = this->_get_item(index.value());
	item.index      = index;
	item.ref_count  = 1;

	// update stats
	this->_stats.total_allocated += 1;
	// recompute memory size here. either last_index or free_indexes was changed
	// above, so this is the right place to change it.
	this->_stats.size = this->_last_index - this->_free_indexes.size();

	return index;
}


/*
 * slab_memory::calloc - allocate memory for N items
 */
template <typename T>
auto
slab_memory<T>::calloc(size_t N) -> std::vector<optional<index_type>>
{
	// TODO: this needs a better implementation than this for loop
	std::vector<optional<index_type>> result;
	for (size_t i = 0; i < N; ++i)
		result.emplace_back(this->alloc());
	return result;
}


/*
 * slab_memory::free - free a memory
 *
 * This 'frees' a memory. Note that in the implementation here, this simply
 * decreases the reference count. If the reference count drops to zero,
 * _memory_release is invoked.
 *
 * Note: It is in the responsibility of the user to call this function when the
 * memory was 'allocated' using memory_new. Any intermediate call to this
 * function can lead to erroneous behavior. Ideally, only the owner of the
 * memory calls memory_new and memory_free, and any intermediate consumer
 * calls memory_incref and memory_decref to indicate additional
 * consumers.
 *
 * Note also that this function must be called in case memory_copy was used.
 */
template <typename T>
auto
slab_memory<T>::free(optional<index_type> index) -> optional<size_t>
{
	//log_verbose("slab_memory::free\n");
	if (!index) {
		//log_warning("call to slab_memory::free with invalid index\n");
		this->_stats.invalid_freed += 1;
		return {};
	}
	//log_verbose("    id = ", index.value(), "\n");

	item_type &item = this->_get_item(index.value());
	size_t ref_count = item.ref_count;
	if (ref_count > 0) {
		ref_count = --(item.ref_count);
		if (ref_count == 0)
			this->_release(index);
	}
	else {
		//log_warning("slab_memory::free called on item with ref_count <= 0\n");
	}

	// statistics to track how many items were called free upon
	this->_stats.total_freed += 1;
	return ref_count;
}


/*
 * slab_memory::incref - increment the reference count of a memory.
 *
 * This function increases the reference count of a memory by 1. To avoid
 * memory leaks, calls to this function should be accompanied by corresponding
 * calls to memory_decref.
 */
template <typename T>
auto
slab_memory<T>::incref(const optional<index_type> index) -> optional<size_t>
{
	if (!index) {
		this->_stats.invalid_incref += 1;
		return {};
	}
	this->_stats.total_incref += 1;
	return ++(this->memory[index.value()].ref_count);
}


/*
 * slab_memory::decref - decrement the reference count of a memory.
 *
 * This function decreases the reference count of a memory by 1. If the
 * reference count drops to 0, the function will release memory assigned to
 * memory by calling _memory_release.
 */
template <typename T>
auto
slab_memory<T>::decref(const optional<index_type> index) -> optional<size_t>
{
	if (!index) {
		this->_stats.invalid_decref += 1;
		return {};
	}
	this->_stats.total_decref += 1;
	return this->free(index);
}


/*
 * slab_memory::copy - copy a memory
 *
 * This will make an exact copy of a memory's values, and return the id of a
 * new memory. Note that this internally calls memory_new, so the caller of
 * memory_copy is considered the owner of the new resource and must either call
 * memory_free accordingly, or make sure that the reference count drops to 0
 * due to appropriate calls of memory_decref.
 */
template <typename T>
auto
slab_memory<T>::copy(const optional<slab_memory_index_t> index) -> optional<index_type>
{
	if (!index)
		return {};

	// NOTE: the order is important here. First allocate, then access the origin
	// value! The reason is that alloc internally can resize the vector, which
	// might move memory and thereby invalidate existing pointers.
	// XXX: this might have changed due to the new paging memory implementation.
	// However, keep the order just to be safe.

	// this initializes reference counting for the new elem
	auto new_index = this->alloc();
	if (!new_index)
		return {};

	item_type &new_item = this->_get_item(new_index.value());
	item_type &origin   = this->_get_item(index.value());

	//log_verbose("slab_memory<T>::copy\n");
	//log_verbose("    origin id        = ", index.value(), "\n");
	//log_verbose("    origin ptr       = ", reinterpret_cast<const void*>(&origin), "\n");
	//log_verbose("    origin value ptr = ", reinterpret_cast<const void*>(&origin.value), "\n");
	//log_verbose("    elem id          = ", new_index.value(), "\n");
	//log_verbose("    elem ptr         = ", reinterpret_cast<const void*>(&new_item), "\n");
	//log_verbose("    elem value ptr   = ", reinterpret_cast<const void*>(&new_item.value), "\n");

	// rely on operator= for the contained value type
	new_item.value = origin.value;
	return new_index;
}


/*
 * slab_memory::_release - release memory given a slab_memory_index
 *
 * This will push the id into the list of free slots that can be used in a
 * subsequent call to `slab_memory::alloc`.
 */
template <typename T>
void
slab_memory<T>::_release(const optional<slab_memory_index_t> index)
{
	if (!index) {
		this->_stats.invalid_released += 1;
		return;
	}
	//log_verbose("slab_memory::_release (id ", index.value(), ")\n");

	// push the ID into the list of free items
	this->_free_indexes.push_back(index.value());
	item_type &item = this->_get_item(index.value());
	item.index     = {};
	item.ref_count = 0;

	// additional statistics
	this->_stats.real_released += 1;
}

/*
 * slab_memory::stats() -- Get some statistics and info for the memory
 */
/*
template <typename T>
void
slab_memory<T>::stats
*/


/*
 * slab_memory_get - get pointer to a certain memory.
 *
 * For details, see `slab_memory::get`.
 */
template <typename T>
inline
const slab_memory_item<T>*
slab_memory_get(slab_memory<T> &slab, slab_memory_index_t pid)
{
	return slab.get(pid);
}


/*
 * slab_memory_alloc - Get a pointer to a new memory item
 *
 * For details, see `slab_memory::alloc`.
 */
template <typename T>
inline
slab_memory_item<T>*
slab_memory_alloc(slab_memory<T> &slab)
{
	return slab.alloc();
}


/*
 * slab_memory_free - free a memory
 *
 * For details, see `slab_memory::free`.
 */
template <typename T>
inline
ptrdiff_t
slab_memory_free(slab_memory<T> &slab, slab_memory_item<T> *item)
{
	return slab.free(item);
}


/*
 * slab_memory_incref - increment the reference count of a memory.
 *
 * For details, see `slab_memory::incref`.
 */
template <typename T>
inline
ptrdiff_t
slab_memory_incref(slab_memory<T> &slab, const slab_memory_index_t id)
{
	return slab.incref(id);
}


/*
 * slab_memory_decref - decrement the reference count of a memory.
 *
 * For details, see `slab_memory::decref`.
 */
template <typename T>
inline
ptrdiff_t
slab_memory_decref(slab_memory<T> &slab, const slab_memory_index_t id)
{
	return slab.decref(id);
}


/*
 * slab_memory_copy - copy a memory
 *
 * For details, see `slab_memory::copy`.
 */
template <typename T>
inline
slab_memory_item<T>*
slab_memory_copy(slab_memory<T> &slab, const slab_memory_index_t id)
{
	return slab.copy(id);
}


/*
 * 'printing' the memory usually means printing the statistics. Provide a
 * convenience function to achieve this goal
 */
template <typename T>
inline
std::ostream&
operator<< (std::ostream &out, const slab_memory<T> &slab)
{
	auto stats = slab.stats();
	out << "Slab Memory Statistics\n";
	out << "    Memory Capacity    " << stats.capacity         << "\n";
	out << "    Memory Size        " << stats.size             << "\n";
	out << "    Page Count         " << stats.page_count       << "\n";
	out << "    Page Size          " << stats.page_size        << "\n";
	out << "    Total Allocated:   " << stats.total_allocated  << "\n";
	out << "    Real Allocated:    " << stats.real_allocated   << "\n";
	out << "    Total Reused:      " << stats.total_reused     << "\n";
	out << "    Total Freed:       " << stats.total_freed      << "\n";
	out << "    Real Released:     " << stats.real_released    << "\n";
	out << "    Invalid Released:  " << stats.invalid_released << "\n";
	out << "    Invalid Freed:     " << stats.invalid_freed    << "\n";
	out << "    Invalid Incref:    " << stats.invalid_incref   << "\n";
	out << "    Invalid Decref:    " << stats.invalid_decref   << "\n";
	out << "    Total Incref:      " << stats.total_incref     << "\n";
	out << "    Total Decref:      " << stats.total_decref     << "\n";
	return out;
}


} // ncr::
