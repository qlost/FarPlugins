#pragma once
#include <CRT\crt.hpp>

template <typename T>
class SimpleVector {
private:
    T* data;
    size_t v_size;
    size_t v_capacity;

    void reallocate(size_t new_capacity) {
        data = (T*)realloc(data, sizeof(T) * new_capacity);
        v_capacity = new_capacity;
    }

public:
    SimpleVector() : data(nullptr), v_size(0), v_capacity(0) {}

    ~SimpleVector() { RemoveAll(); free(data); }

    T* Data() {return data;}

    void Add(const T& value) {
        if (v_size >= v_capacity) {
            reallocate(v_capacity == 0 ? 1 : v_capacity * 2);
        }
        data[v_size++] = value;
    }

    void RemoveAll() {
        for (size_t i = 0; i < v_size; ++i)
            delete data[i];
        v_size = 0;
    }

    T& operator[](size_t index) { return data[index]; }
    size_t size() const { return v_size; }
};
