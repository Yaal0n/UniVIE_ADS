#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>

template <typename Key, size_t N = 8>
class ADS_set {
public:
    class Iterator;
    using value_type = Key;
    using key_type = Key;
    using reference = key_type &;
    using const_reference = const key_type &;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = Iterator;
    using const_iterator = Iterator;
    using key_compare = std::less<key_type>;    // B+-Tree
    using key_equal = std::equal_to<key_type>;  // Hashing
    using hasher = std::hash<key_type>;         // Hashing

private:
    struct Block;
    size_type nexttosplit {0};
    size_type d {0};
    size_type curr_size {0};
    size_type table_size {0};
    size_type block_counter{0};
    Block **entries {nullptr};

public:
    ADS_set(): nexttosplit{0}, d{2}, curr_size{0}, table_size{8}, block_counter{4}, entries{new Block*[table_size]} {
        for(size_type x{0}; x < block_counter; ++x) entries[x] = new Block;
    }                                                           // PH1
    ADS_set(std::initializer_list<key_type> ilist): ADS_set {std::begin(ilist), std::end(ilist)} {}
    template<typename InputIt> ADS_set(InputIt first, InputIt last): ADS_set{} {
        insert(first,last);
    }
    ADS_set(const ADS_set &other){
        operator=(other);
    }

    ~ADS_set(){
        for(size_type x{0}; x < block_counter; ++x){
            delete entries[x];
        }
        delete[] entries;
        entries = nullptr;
        nexttosplit = 0;
        d = 2;
        curr_size = 0;
        table_size = 8;
        block_counter = 4;
    }

    ADS_set &operator=(const ADS_set &other) {
        if(other.empty()) {
            ADS_set result;
            swap(result);
            return *this;
        }
        ADS_set result2;
        for(auto t = other.begin(); t != other.end(); t++)
            result2.insertkey(*t);
        swap(result2);
        return *this;
    }

    ADS_set &operator=(std::initializer_list<key_type> ilist){
        ADS_set temp;
        temp.insert(ilist);
        swap(temp);
        return *this;
    }

    size_type size() const {
        return curr_size;
    }// PH1
    
    bool empty() const {
        return !curr_size;
    }                                                  // PH1

    void clear() {
        ADS_set empty;
        swap(empty);
    }

    void insert(std::initializer_list<key_type> ilist) {
        insert(std::begin(ilist), std::end(ilist));
    }                  // PH1

    std::pair<iterator,bool> insert(const key_type &key){
        if(count(key)) {
            return std::make_pair(find(key), false);
        }
        else {
            insertkey(key);
            return std::make_pair(find(key), true);
        }
    }

    void insertkey (const key_type &key) {
        if (count(key) == 0) {
            curr_size++;
            entries[hash(key)]->block_einfuegen(key);
            if(entries[hash(key)]->check_split())
                split();
        }
        else
            return;
    }

    template<typename InputIt> void insert(InputIt first, InputIt last) {
        while(first != last) {
            if (count(*first) == 0) {
                curr_size++;
                entries[hash(*first)]->block_einfuegen(*first);
                if(entries[hash(*first)]->check_split())
                    split();
            }
            first++;
        }
    } // PH1
    
    void enlargetable() {
        if(block_counter == table_size) {
            size_type new_table_size{2*table_size};
            Block **new_entries{new Block*[new_table_size]};
            for(size_type y {0}; y < block_counter; ++y) {
                new_entries[y] = entries[y];
            }
            delete[] entries;
            entries = new_entries;
            table_size = new_table_size;
        }
        entries[block_counter] = new Block;
        block_counter++;
    }
    
    void split() {
        enlargetable();
        Block *a = entries[nexttosplit];
        size_type nts = nexttosplit;
        entries[nexttosplit] = new Block;
        
        size_type temp = 1<<d;

        if(nexttosplit == temp){
            d++;
            nexttosplit = 0;
        }
        else {
            ++nexttosplit;
        }
        if(a->n > 0)
            rehash(a, nts);
        delete a;
    }

    void rehash(Block *A, size_type nts){
        for(size_type x{0}; x < A->n; x++){
            entries[hash(A->values[x])]->block_einfuegen(A->values[x]);
        }
        if(A->of != nullptr){
            return rehash(A->of, nts);
        }
        return;
    }

    size_type hash(const key_type &n) const {
        size_type result = hasher{}(n)%(1<<d);
        if(result < nexttosplit)
            return hash_2(n);
        else
            return result;
    }
    
    size_type hash_2(const key_type &n) const {
        size_type result = hasher{}(n)%(1<<(d+1));
        return result;
    }

    size_type erase (const key_type &key) {
        Block *foundblock{nullptr};
        Block *lastblock{nullptr};
        size_type index;

        if (!count(key))
            return 0;
        else {
                foundblock = entries[hash(key)]->find_key(key);
                index = foundblock->blockindexfinder(key);
                lastblock = entries[hash(key)];
                while(lastblock->of != nullptr)
                    lastblock = lastblock->of;
                foundblock->values[index] = lastblock->values[lastblock->n - 1];
                lastblock->n--;
                if (lastblock->n == 0 && lastblock != entries[hash(key)]) {
                    entries[hash(key)]->deleteof();
                    lastblock = nullptr;
                }
        }
        curr_size--;
        return 1;
    }

    size_type count(const key_type &key) const {
        if (curr_size == 0)
            return 0;
        return entries[hash(key)]->block_finden(key) == nullptr ? 0 : 1;
    }
    
    iterator find(const key_type &key) const {
        Block *block;
        key_type *keyptr;
        size_type blockvalueindex;
        
        if(!count(key))
            return end();
        block = entries[hash(key)]->find_key(key);
        blockvalueindex = block->blockindexfinder(key);
        keyptr = &block->values[blockvalueindex];
        Iterator temp(this, block, keyptr, hash(key), blockvalueindex);
        return temp;
    }

    void swap(ADS_set &other){
        std::swap(nexttosplit, other.nexttosplit);
        std::swap(curr_size, other.curr_size);
        std::swap(table_size, other.table_size);
        std::swap(block_counter, other.block_counter);
        std::swap(entries, other.entries);
        std::swap(d, other.d);
    }

    const_iterator begin() const {
        if(curr_size == 0)
            return end();

        Block *foundblock = entries[0];
        size_t tableindex{0};
        if(entries[0]->n == 0) {
            for(size_t x{0}; x < block_counter; ++x){
                if(entries[x]->n > 0){
                    foundblock = entries[x];
                    tableindex = x;
                    break;
                }
            }
        }
        Iterator begin(this, foundblock, &foundblock->values[0], tableindex, 0);
        return begin;
    }
    
    const_iterator end() const {
        return Iterator(this);
    }

    void dump(std::ostream &o = std::cerr) const {
        o << "Linear Hashing <" << typeid(Key).name() << ", " << N << "> curr_size = " << curr_size << " table_size = "
          << table_size << " block_counter = " << block_counter <<  "\n";
        for (size_t i{0}; i < block_counter; ++i) {
            o << "Block " << i << ": \n";
            for (size_t x{0}; x < entries[i]->n ; ++x) {
                if(entries[i]->n > 0)
                    o << "[" << entries[i]->values[x] << "]";
            }
            if(entries[i]->of != nullptr) {
                    o << "-->";
                    for(size_t z {0}; z < entries[i]->of->n; ++z) {
                        o << "[" << entries[i]->of->values[z] << "]";
                    }
                    if (entries[i]->of->of != nullptr) {
                        o << "-->";
                        for(size_t z {0}; z < entries[i]->of->of->n; ++z) {
                            o << "[" << entries[i]->of->of->values[z] << "]";
                        }
                        if (entries[i]->of->of->of != nullptr) {
                            o << "-->";
                            for(size_t z {0}; z < entries[i]->of->of->of->n; ++z) {
                                o << "[" << entries[i]->of->of->of->values[z] << "]";
                            }
                        }
                    }
            }
            o << "\n";
        }
        o << "NTS: " << nexttosplit << ", d: " << d;
    }

    friend bool operator == (const ADS_set &lhs, const ADS_set &rhs) {
        if(lhs.size() != rhs.size())
            return false;
        else {
            if(lhs.entries != nullptr) {
                for(size_type x{0}; x < lhs.block_counter; ++x) {
                    for (size_type y{0}; y < lhs.entries[x]->n; ++y) {
                        if (rhs.count(lhs.entries[x]->values[y]) == 0)
                            return false;
                    }
                }
            }
        }
        return true;
    }


    friend bool operator!=(const ADS_set &lhs, const ADS_set &rhs) {
        return !(lhs == rhs);
    }
};

template <typename Key, size_t N>
struct ADS_set <Key,N>::Block{
    size_t n;
    key_type values[N];
    Block *of;

    Block() {
        n = 0;
        of = nullptr;
    }

    ~Block() {
        if(of != nullptr)
            delete of;
    }

    void block_einfuegen(const key_type &k) {
        if(n == N) {
            if(of == nullptr)
                of = new Block;
            return of->block_einfuegen(k);
        }
        values[n] = k;
        n++;
    }

    const key_type *block_finden(const key_type &k) {
        for (size_type i{0}; i < n; ++i) {
            if (key_equal{}(k, values[i]))
                return &values[i];
        }
        if(of != nullptr)
            return of->block_finden(k);

        return nullptr;
    }

    Block *find_key(const key_type &key) {
        for(size_type i{0}; i < n; ++i) {
            if(key_equal{}(key, values[i])){
                return this;
            }
        }
        return of->find_key(key);
    }

    void deleteof() {
        if(of->of != nullptr)
            return of->deleteof();
        if(of->of == nullptr){
            delete of;
            of = nullptr;
        }
    }

    size_type blockindexfinder(const key_type &key) {
        size_type i = 0;
        for(; i < n; ++i) {
            if(key_equal{}(key, values[i]))
                break;
        }
        return i;
    }

    bool check_split() {
        if(of == nullptr)
            return false;
        if(of != nullptr && of->n == 1){
            return true;
        }
        else
            return of->check_split();
    }

};

template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;

private:
    const ADS_set *set;
    Block *block;
    pointer keyptr;
    size_t tableindex;
    size_t blockvalueindex;

public:
    Iterator(){
        set = nullptr;
        block = nullptr;
        keyptr = nullptr;
        tableindex = 0;
        blockvalueindex = 0;
    }
    
    Iterator(const ADS_set *set): set(set) {
        block = nullptr;
        keyptr = nullptr;
        tableindex = set->block_counter;
        blockvalueindex = 0;
    }

    Iterator(const ADS_set *set,Block *block, pointer keyptr, size_t tableindex, size_t blockvalueindex): set(set), block(block), keyptr(keyptr), tableindex(tableindex), blockvalueindex(blockvalueindex) {}

    reference operator*() const {
        return *keyptr;
    }

    pointer operator->() const {
        return keyptr;
    }

    Iterator &operator++() {
        ++blockvalueindex;
        while(block->n == blockvalueindex) {
            if(block->of == nullptr) {
                if(tableindex+1 == set->block_counter) {
                    block = nullptr;
                    keyptr = nullptr;
                    tableindex = set->block_counter;
                    blockvalueindex = 0;
                    return *this;
                }
                ++tableindex;
                block = set->entries[tableindex];
            }
            else
                block = block->of;
            blockvalueindex = 0;
        }
        keyptr = block->values+blockvalueindex;
        return *this;
    }

    Iterator operator++(int) {
        Iterator result(set, block, keyptr, tableindex, blockvalueindex);
        operator++ ();
        return result;
    }

    friend bool operator==(const Iterator &lhs, const Iterator &rhs) {
        return (lhs.set == rhs.set && lhs.block == rhs.block && lhs.keyptr == rhs.keyptr && lhs.tableindex == rhs.tableindex && lhs.blockvalueindex == rhs.blockvalueindex);
    }

    friend bool operator!=(const Iterator &lhs, const Iterator &rhs) {
        return !(lhs == rhs);
    }
};

template <typename Key, size_t N> void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) {lhs.swap(rhs);}

#endif // ADS_SET_H
