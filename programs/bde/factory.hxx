#pragma once

template<typename T>
class factory_unclosable_item {
    T target;
    public:
    factory_unclosable_item() = default;
    ~factory_unclosable_item() = default;

    template<class... _Args>
    factory_unclosable_item(_Args&&... args): target(args...) {

    }

    operator T&(void) {
        return target;
    }

    void close(void) {

    }
};


// non thread safe class
template<typename T, typename Id_T=int>
class factory {
private:
    T* buffer = 0;
    int sz = 0;
public:
    factory() {
    }

    T& operator[](int idx) {
        assert(idx >= 0);
        assert(idx < sz);

        return buffer[idx];
    }

    T* find_id(Id_T id) {
        for(int i = 0; i < sz; i++)
            if(buffer[i]->id == id)
                return &buffer[i];
        return NULL;
    }

    template<class... _Args>
    void emplace_back(_Args&&... args) {
        buffer = (T*)realloc((void*)buffer, (sz + 1) * sizeof(T));
        memset(&buffer[sz], 0, sizeof(T));
        buffer[sz] = T(args...);
        sz++;
    }

    using iterator = T*;

    void remove(int idx) {
        assert(idx >= 0);
        assert(idx < sz);
        (buffer + idx)->close();
        memmove(
            buffer + idx,
            buffer + idx + 1,
            (sz - idx - 1) * sizeof(T)
        );
        sz--;
    }
    void remove(iterator it) {
        remove(it - buffer);
    }

    iterator begin() {
        return buffer;
    }
    iterator end() {
        return buffer + sz;
    }

    ~factory() {
        for(int i = 0; i < sz; i++)
            buffer[i].close();
        
        if(buffer)
            free(buffer);
    }
};
