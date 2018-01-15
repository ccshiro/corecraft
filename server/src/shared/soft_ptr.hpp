/*
 * Copyright (C) 2014 corecraft <https://www.worldofcorecraft.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SOFT_PTR_HPP
#define SOFT_PTR_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <type_traits>

// soft_ptr: Provides weak_ptr like behavior for memory you manage yourself.
//           If you have the option to use shared_ptr/weak_ptr instead, do so.
//           soft_ptr is only for when you have no choice but to manually manage
//           the memory, but still want weak references.

// WARNING: A soft_ptr says nothing about the validity of the object after your
//          call to soft_ptr::get(). It only assures that the memory is valid at
//          the time you call soft_ptr::get(). It's your responsbility to make
//          sure the object remains valid after your soft_ptr::get() call.

// usage example
/*
class Object : public enable_soft_from_this<Object> { };
Object* o = new Object{};
auto soft = o->soft_from_this();
if (auto ptr = soft.get()) // returns pointer to Object
    ...
delete o;
if (auto ptr = soft.get()) // returns nullptr, Object is deleted
    ...
*/

// implementation

// === control block ===

constexpr uint32_t _cb_memory_exists = 1 << 31;

struct _control_block_cnt { uint32_t cnt; };
struct _control_block_atomic_cnt { std::atomic<uint32_t> cnt; };

template<typename T, bool Atomic>
class _control_block : public std::conditional<Atomic,
        _control_block_atomic_cnt, _control_block_cnt>::type
{
private:
    _control_block() { }
    ~_control_block() { }

public:
    static _control_block* create() { return new _control_block; }

    // uint32_t cnt;
    T* mem;

    void incr()
    {
        assert((this->cnt & ~_cb_memory_exists) < uint32_t(1 << 31) - 1);
        ++this->cnt;
    }
    void decr()
    {
        assert(this->cnt > 0);
        unsigned count = --this->cnt;
        if (count == 0)
            delete this;
    }
};

// === soft_ptr ===

template<typename T, bool A>
class soft_ptr
{
    template<typename T_, bool A_>
    friend class enable_soft_from_this;

    soft_ptr(_control_block<T, A>* block) : block_{block} { block_->incr(); }

public:
    soft_ptr() : block_{nullptr} { }
    ~soft_ptr()
    {
        if (block_)
            block_->decr();
    }

    // copy
    soft_ptr(const soft_ptr& p) : block_{p.block_}
    {
        if (block_)
            block_->incr();
    }
    soft_ptr& operator=(const soft_ptr& p)
    {
        if (block_)
            block_->decr();
        block_ = p.block_;
        if (block_)
            block_->incr();
        return *this;
    }

    // move
    soft_ptr(soft_ptr&& p)
        : block_{nullptr}
    {
        std::swap(block_, p.block_);
    }
    soft_ptr& operator=(soft_ptr&& p)
    {
        std::swap(block_, p.block_);
    }

    // access
    T* get() const
    {
        return block_->cnt & _cb_memory_exists ? block_->mem : nullptr;
    }

private:
    _control_block<T, A>* block_;
};

// === enable_soft_from_this ===

template<typename T, bool Atomic = false>
class enable_soft_from_this
{
public:
    enable_soft_from_this(enable_soft_from_this&) = delete;
    enable_soft_from_this& operator=(enable_soft_from_this&) = delete;

    soft_ptr<T, Atomic> soft_from_this()
    {
        return soft_ptr<T, Atomic>(block_);
    }

protected:
    enable_soft_from_this() : block_{_control_block<T, Atomic>::create()}
    {
        block_->cnt = 1 | _cb_memory_exists;
        block_->mem = static_cast<T*>(this);
    }
    virtual ~enable_soft_from_this()
    {
        block_->cnt &= ~_cb_memory_exists;
        block_->decr();
    }

private:
    _control_block<T, Atomic>* block_;
};

#endif
