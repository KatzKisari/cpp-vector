#pragma once


#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>





template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(other.capacity_) 
    {
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);

        return *this;
    }


    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};





template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;


    Vector() = default;


    explicit Vector(size_t size)
        : data_(size)
        , size_(size) 
    {
        std::uninitialized_value_construct_n(begin(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.begin(), other.Size(), begin());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) 
    { 
        other.size_ = 0;
    }



    ~Vector() {
        std::destroy_n(begin(), size_);
    }





    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }


    T& Back() noexcept {
        return *std::prev(end());
    }
    const T& Back() const noexcept {
        return *std::prev(cend());
    }
    T& Front() noexcept {
        return *begin();
    }
    const T& Front() const noexcept {
        return *cbegin();
    }




    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> new_vector(rhs);
                Swap(new_vector);
            }
            else {
                std::copy_n(rhs.data_.GetAddress(), std::min(Size(), rhs.Size()), data_.GetAddress());

                if (rhs.Size() < Size()) {
                    std::destroy_n((data_.GetAddress() + rhs.Size()), (Size() - rhs.Size()));
                }
                else {
                    std::uninitialized_copy_n((rhs.data_.GetAddress() + Size()), (rhs.Size() - Size()), (data_.GetAddress() + Size()));
                }

                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = rhs.size_;
        rhs.size_ = 0;

        return *this;
    }


    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }


    void Reserve(size_t new_capacity) {
        if (new_capacity <= Capacity()) return;
        
        RawMemory<T> new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) 
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        else 
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }



    void Resize(size_t new_size) {
        if (new_size == size_) return;

        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }

        size_ = new_size;
    }




    void PushBack(const T& value) {
        if (Size() == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data + size_) T(value);
            UninitializedMoveOrCopyN(begin(), size_, new_data.GetAddress());


            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(value);
        }

        ++size_;
    }

    void PushBack(T&& value) {
        if (Size() == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data + size_) T(std::move(value));
            UninitializedMoveOrCopyN(begin(), size_, new_data.GetAddress());

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::move(value));
        }

        ++size_;
    }





    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());

        iterator non_const_pos = data_ + (pos - begin());
        iterator it = non_const_pos;

        if (Size() == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            it = new_data + (pos - begin());
            new (it) T(std::forward<Args>(args)...);


            UninitializedMoveOrCopy(begin(), non_const_pos, new_data.GetAddress());
            UninitializedMoveOrCopy(non_const_pos, end(), std::next(it));

            std::destroy_n(begin(), size_);
            data_.Swap(new_data);
        }
        else {
            if (pos != end()) {
                new (data_ + size_) T(std::move(Back()));
                std::move_backward(non_const_pos, std::prev(end()), end());

                *it = std::move(T(std::forward<Args>(args)...));
            }
            else {
                new (data_ + size_) T(std::forward<Args>(args)...);
            }
        }

        ++size_;

        return it;
    }



    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Size() == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data + size_) T(std::forward<Args>(args)...);
            UninitializedMoveOrCopyN(begin(), size_, new_data.GetAddress());

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }

        ++size_;

        return Back();
    }




    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }




    void PopBack() noexcept {
        assert(size_ != 0);
        std::destroy_at(data_.GetAddress() + (--size_));
    }


    iterator Erase(const_iterator pos) noexcept {
        assert(pos >= begin() && pos < end());
        iterator it = data_ + (pos - begin());

        std::move(std::next(it), end(), it);
        PopBack();

        return it;
    }


private:
    RawMemory<T> data_;
    size_t size_ = 0;


    template <typename Iterator>
    static void UninitializedMoveOrCopy(Iterator begin, Iterator end, Iterator destination) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            std::uninitialized_move(begin, end, destination);
        else
            std::uninitialized_copy(begin, end, destination);
    }

    template <typename Iterator>
    static void UninitializedMoveOrCopyN(Iterator begin, size_t num, Iterator destination) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            std::uninitialized_move_n(begin, num, destination);
        else
            std::uninitialized_copy_n(begin, num, destination);
    }
};