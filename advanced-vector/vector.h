#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

//-----------------------------------RAW MEMORY-----------------------------------
template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = rhs.buffer_;
        capacity_ = rhs.capacity_;
        rhs.buffer_ = nullptr;
        rhs.capacity_ = 0;
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
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

//-----------------------------------VECTOR-----------------------------------
template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector()
        : data_(0)
        , size_(0)
    {
    }
    
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.Size(), data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        :size_(other.size_)
    {
        data_ = std::move(other.data_);
    }

    ~Vector() {
        if (data_.GetAddress() != nullptr) {
            std::destroy_n(data_.GetAddress(), size_);
        }
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(Size(), rhs.Size()), data_.GetAddress());
                if (rhs.Size() > Size()) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + Size(), rhs.Size() - Size(), data_.GetAddress() + Size());
                }
                else {
                    std::destroy_n(data_.GetAddress() + rhs.Size(), Size() - rhs.Size());
                }
                size_ = rhs.Size();
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) {
        data_ = std::exchange(rhs.data_, {});
        size_ = rhs.size_;
        return *this;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        MoveElements(data_.GetAddress(), size_, new_data.GetAddress());
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(end(), std::forward<Args>(args)...);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        T* new_elem;
        if (size_ == data_.Capacity()) {
            new_elem = EmplaceWithReallocation(pos, std::forward<Args>(args)...);
        }
        else {
            new_elem = EmplaceWithoutReallocation(pos, std::forward<Args>(args)...);
        }
        return new_elem;
    }

    iterator Erase(const_iterator pos){
        iterator current_pos = begin() + (pos - begin());
        std::move(current_pos + 1, end(), current_pos);
        PopBack();
        return current_pos;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    template <typename... Args>
    iterator EmplaceWithReallocation(const_iterator pos, Args&&... args) {
        T* new_elem;
        size_t current_pos = pos - begin();
        //Выделяем новый блок сырой памяти с удвоенной вместимостью
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        //Конструируем вставляемый элемент
        new_elem = new (new_data + current_pos) T(std::forward<Args>(args)...);

        if (size_ > 0) { //Если вектор пустой, перемещать нечего
            MoveElements(data_.GetAddress(), current_pos, new_data.GetAddress());
            MoveElements(data_.GetAddress() + current_pos, size_ - current_pos, new_data.GetAddress() + current_pos + 1);
        }

        data_.Swap(new_data);
        ++size_;
        return new_elem;
    }

    template <typename... Args>
    iterator EmplaceWithoutReallocation(const_iterator pos, Args&&... args) {
        T* new_elem;
        size_t current_pos = pos - begin();
        if (size_ > 0) { //Если вектор пустой, перемещать нечего
            if (current_pos != size_) {
                //Перемещаем последний элемент
                new (data_ + size_) T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + current_pos - 1, end() - 1, end());
                std::destroy_at(data_ + current_pos);
            }
        }
        new_elem = new (data_ + current_pos) T(std::forward<Args>(args)...);
        ++size_;
        return new_elem;
    }

    void MoveElements(T* data_from_ptr, size_t elements_cnt, T* new_data_to_ptr) {
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_from_ptr, elements_cnt, new_data_to_ptr);
        }
        else {
            std::uninitialized_copy_n(data_from_ptr, elements_cnt, new_data_to_ptr);
        }
        std::destroy_n(data_from_ptr, elements_cnt);
    }
};