/**
 * MIT License
 * 
 * Copyright (c) 2017 Tessil
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef TSL_ROBIN_HASH_H
#define TSL_ROBIN_HASH_H 


#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>



#ifndef tsl_assert
    #ifdef TSL_DEBUG
    #define tsl_assert(expr) assert(expr)
    #else
    #define tsl_assert(expr) (static_cast<void>(0))
    #endif
#endif



namespace tsl {


/**
 * Grow the map by a multiple of two keeping bucket_count to a power of two. It allows
 * the map to use a mask operation instead of a modulo operation to map a hash to a bucket.
 * 
 * GrowthFactor must be a power of two >= 2.
 */
template<std::size_t GrowthFactor>
class power_of_two_growth_policy_rh {
public:
    /**
     * Called on map creation and rehash. The number of buckets requested is passed by parameter.
     * This number is a minimum, the policy may update this value with a higher value if needed.
     */
    power_of_two_growth_policy_rh(std::size_t& min_bucket_count_in_out) {
        if(min_bucket_count_in_out > max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        static_assert(MIN_BUCKETS_SIZE > 0, "");
        const std::size_t min_bucket_count = MIN_BUCKETS_SIZE;
        
        min_bucket_count_in_out = std::max(min_bucket_count, min_bucket_count_in_out);
        min_bucket_count_in_out = round_up_to_power_of_two(min_bucket_count_in_out);
        m_mask = min_bucket_count_in_out - 1;
    }
    
    /**
     * Return the bucket [0, bucket_count()) to which the hash belongs.
     */
    std::size_t bucket_for_hash(std::size_t hash) const {
        return hash & m_mask;
    }
    
    /**
     * Return the bucket count to uses when the bucket array grows on rehash.
     */
    std::size_t next_bucket_count() const {
        if((m_mask + 1) > max_bucket_count() / GrowthFactor) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        return (m_mask + 1) * GrowthFactor;
    }
    
    /**
     * Return the maximum number of buckets supported by the policy.
     */
    std::size_t max_bucket_count() const {
        return std::numeric_limits<std::size_t>::max() / GrowthFactor + 1;
    }
    
private:
    static std::size_t round_up_to_power_of_two(std::size_t value) {
        if(value == 0) {
            return 1;
        }
        
        if(is_power_of_two(value)) {
            return value;
        }
            
        --value;
        for(std::size_t i = 1; i < sizeof(std::size_t) * CHAR_BIT; i *= 2) {
            value |= value >> i;
        }
        
        return value + 1;
    }
    
    static constexpr bool is_power_of_two(std::size_t value) {
        return value != 0 && (value & (value - 1)) == 0;
    }
    
protected:
    static const std::size_t MIN_BUCKETS_SIZE = 2;
    static_assert(is_power_of_two(GrowthFactor) && GrowthFactor >= 2, "GrowthFactor must be a power of two >= 2.");
    
    std::size_t m_mask;
};

/**
 * Grow the map by GrowthFactor::num/GrowthFactor::den and use a modulo to map a hash
 * to a bucket. Slower but it can be usefull if you want a slower growth.
 */
template<class GrowthFactor = std::ratio<3, 2>>
class mod_growth_policy_rh {
public:
    mod_growth_policy_rh(std::size_t& min_bucket_count_in_out) {
        if(min_bucket_count_in_out > max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        static_assert(MIN_BUCKETS_SIZE > 0, "");
        const std::size_t min_bucket_count = MIN_BUCKETS_SIZE;
        
        min_bucket_count_in_out = std::max(min_bucket_count, min_bucket_count_in_out);
        m_bucket_count = min_bucket_count_in_out;
    }
    
    std::size_t bucket_for_hash(std::size_t hash) const {
        tsl_assert(m_bucket_count != 0);
        return hash % m_bucket_count;
    }
    
    std::size_t next_bucket_count() const {
        if(m_bucket_count == max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        const double next_bucket_count = std::ceil(double(m_bucket_count) * REHASH_SIZE_MULTIPLICATION_FACTOR);
        if(!std::isnormal(next_bucket_count)) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        if(next_bucket_count > double(max_bucket_count())) {
            return max_bucket_count();
        }
        else {
            return std::size_t(next_bucket_count);
        }
    }
    
    std::size_t max_bucket_count() const {
        return MAX_BUCKET_COUNT;
    }
    
private:
    static const std::size_t MIN_BUCKETS_SIZE = 2;
    static constexpr double REHASH_SIZE_MULTIPLICATION_FACTOR = 1.0*GrowthFactor::num/GrowthFactor::den;
    static const std::size_t MAX_BUCKET_COUNT = 
            std::size_t(double(
                    std::numeric_limits<std::size_t>::max()/REHASH_SIZE_MULTIPLICATION_FACTOR
            ));
            
    static_assert(REHASH_SIZE_MULTIPLICATION_FACTOR >= 1.1, "Growth factor should be >= 1.1.");
    
    std::size_t m_bucket_count;
};



namespace detail_robin_hash {

static constexpr const std::array<std::size_t, 39> PRIMES = {{
    5ul, 17ul, 29ul, 37ul, 53ul, 67ul, 79ul, 97ul, 131ul, 193ul, 257ul, 389ul, 521ul, 769ul, 1031ul, 1543ul, 2053ul, 
    3079ul, 6151ul, 12289ul, 24593ul, 49157ul, 98317ul, 196613ul, 393241ul, 786433ul, 1572869ul, 3145739ul, 
    6291469ul, 12582917ul, 25165843ul, 50331653ul, 100663319ul, 201326611ul, 402653189ul, 805306457ul, 
    1610612741ul, 3221225473ul, 4294967291ul
}};

template<unsigned int IPrime>
static std::size_t mod(std::size_t hash) { return hash % PRIMES[IPrime]; }

// MOD_PRIME[iprime](hash) returns hash % PRIMES[iprime]. This table allows for faster modulo as the
// compiler can optimize the modulo code better with a constant known at the compilation.
static constexpr const std::array<std::size_t(*)(std::size_t), 39> MOD_PRIME = {{ 
    &mod<0>, &mod<1>, &mod<2>, &mod<3>, &mod<4>, &mod<5>, &mod<6>, &mod<7>, &mod<8>, &mod<9>, &mod<10>, 
    &mod<11>, &mod<12>, &mod<13>, &mod<14>, &mod<15>, &mod<16>, &mod<17>, &mod<18>, &mod<19>, &mod<20>, 
    &mod<21>, &mod<22>, &mod<23>, &mod<24>, &mod<25>, &mod<26>, &mod<27>, &mod<28>, &mod<29>, &mod<30>, 
    &mod<31>, &mod<32>, &mod<33>, &mod<34>, &mod<35>, &mod<36>, &mod<37> , &mod<38>
}};

}

/**
 * Grow the map by using prime numbers as size. Slower than tsl::power_of_two_growth_policy_rh in general 
 * but will probably distribute the values around better in the buckets with a poor hash function.
 * 
 * To allow the compiler to optimize the modulo operation, a lookup table is used with constant primes numbers.
 * 
 * With a switch the code would look like:
 * \code
 * switch(iprime) { // iprime is the current prime of the hash table
 *     case 0: hash % 5ul;
 *             break;
 *     case 1: hash % 17ul;
 *             break;
 *     case 2: hash % 29ul;
 *             break;
 *     ...
 * }    
 * \endcode
 * 
 * Due to the constant variable in the modulo the compiler is able to optimize the operation
 * by a series of multiplications, substractions and shifts. 
 * 
 * The 'hash % 5' could become something like 'hash - (hash * 0xCCCCCCCD) >> 34) * 5' in a 64 bits environement.
 */
class prime_growth_policy_rh {
public:
    prime_growth_policy_rh(std::size_t& min_bucket_count_in_out) {
        auto it_prime = std::lower_bound(tsl::detail_robin_hash::PRIMES.begin(), 
                                         tsl::detail_robin_hash::PRIMES.end(), min_bucket_count_in_out);
        if(it_prime == tsl::detail_robin_hash::PRIMES.end()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        m_iprime = static_cast<unsigned int>(std::distance(tsl::detail_robin_hash::PRIMES.begin(), it_prime));
        min_bucket_count_in_out = *it_prime;
    }
    
    std::size_t bucket_for_hash(std::size_t hash) const {
        return bucket_for_hash_iprime(hash, m_iprime);
    }
    
    std::size_t next_bucket_count() const {
        if(m_iprime + 1 >= tsl::detail_robin_hash::PRIMES.size()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        return tsl::detail_robin_hash::PRIMES[m_iprime + 1];
    }   
    
    std::size_t max_bucket_count() const {
        return tsl::detail_robin_hash::PRIMES.back();
    }
    
private:  
    std::size_t bucket_for_hash_iprime(std::size_t hash, unsigned int iprime) const {
        tsl_assert(iprime < tsl::detail_robin_hash::MOD_PRIME.size());
        return tsl::detail_robin_hash::MOD_PRIME[iprime](hash);
    }
    
private:
    unsigned int m_iprime;
};


    
namespace detail_robin_hash {

template<typename T>
struct make_void {
    using type = void;
};

template<typename T, typename = void>
struct has_is_transparent: std::false_type {
};

template<typename T>
struct has_is_transparent<T, typename make_void<typename T::is_transparent>::type>: std::true_type {
};



using truncated_hash_type = std::uint_least32_t;

template<bool StoreHash>
class bucket_entry_hash {
public:
    bool bucket_hash_equal(std::size_t /*hash*/) const noexcept {
        return true;
    }
    
    truncated_hash_type truncated_hash() const noexcept {
        return 0;
    }
    
protected:
    void set_hash(truncated_hash_type /*hash*/) noexcept {
    }
};

template<>
class bucket_entry_hash<true> {
public:
    bool bucket_hash_equal(std::size_t hash) const noexcept {
        return m_hash == truncated_hash_type(hash);
    }
    
    truncated_hash_type truncated_hash() const noexcept {
        return m_hash;
    }
    
protected:
    void set_hash(truncated_hash_type hash) noexcept {
        m_hash = truncated_hash_type(hash);
    }
    
private:    
    truncated_hash_type m_hash;
};


/**
 * dist_from_init_bucket() is >= 0 iff the bucket_entry is not empty.
 * If the bucket_entry is empty, dist_from_init_bucket() is < 0.
 */
template<typename ValueType, bool StoreHash>
class bucket_entry: public bucket_entry_hash<StoreHash> {
    using bucket_hash = bucket_entry_hash<StoreHash>;
    
public:
    using value_type = ValueType;
    using distance_type = std::int_least16_t;
    
    
    bucket_entry() noexcept: bucket_hash(), m_dist_from_initial_bucket(EMPTY_MARKER_DIST_FROM_INIT_BUCKET),
                             m_last_bucket(false)
    {
        tsl_assert(empty());
    }
    
    bucket_entry(const bucket_entry& other) noexcept(std::is_nothrow_copy_constructible<value_type>::value): 
            bucket_hash(other),
            m_dist_from_initial_bucket(EMPTY_MARKER_DIST_FROM_INIT_BUCKET), 
            m_last_bucket(other.m_last_bucket) 
    {
        if(!other.empty()) {
            ::new (static_cast<void*>(std::addressof(m_value))) value_type(other.value());
            m_dist_from_initial_bucket = other.m_dist_from_initial_bucket;
        }
    }
    
    bucket_entry(bucket_entry&& other) noexcept(std::is_nothrow_move_constructible<value_type>::value): 
            bucket_hash(std::move(other)),
            m_dist_from_initial_bucket(EMPTY_MARKER_DIST_FROM_INIT_BUCKET), 
            m_last_bucket(other.m_last_bucket) 
    {
        if(!other.empty()) {
            ::new (static_cast<void*>(std::addressof(m_value))) value_type(std::move(other.value()));
            m_dist_from_initial_bucket = other.m_dist_from_initial_bucket;
        }
    }
    
    bucket_entry& operator=(const bucket_entry& other) 
            noexcept(std::is_nothrow_copy_constructible<value_type>::value) 
    {
        if(this != &other) {
            clear();
            
            bucket_hash::operator=(other);
            if(!other.empty()) {
                ::new (static_cast<void*>(std::addressof(m_value))) value_type(other.value());
            }
            
            m_dist_from_initial_bucket = other.m_dist_from_initial_bucket;
            m_last_bucket = other.m_last_bucket;
        }
        
        return *this;
    }
    
    bucket_entry& operator=(bucket_entry&& ) = delete;
    
    ~bucket_entry() noexcept {
        clear();
    }
    
    void clear() noexcept {
        if(!empty()) {
            destroy_value();
            m_dist_from_initial_bucket = EMPTY_MARKER_DIST_FROM_INIT_BUCKET;
        }
    }
    
    bool empty() const noexcept {
        return m_dist_from_initial_bucket == EMPTY_MARKER_DIST_FROM_INIT_BUCKET;
    }
    
    value_type& value() noexcept {
        tsl_assert(!empty());
        return *reinterpret_cast<value_type*>(std::addressof(m_value));
    }
    
    const value_type& value() const noexcept {
        tsl_assert(!empty());
        return *reinterpret_cast<const value_type*>(std::addressof(m_value));
    }
    
    distance_type dist_from_init_bucket() const noexcept {
        return m_dist_from_initial_bucket;
    }
    
    distance_type& dist_from_init_bucket_ref() noexcept {
        return m_dist_from_initial_bucket;
    }
    
    bool last_bucket() const noexcept {
        return m_last_bucket;
    }
    
    void set_as_last_bucket() noexcept {
        m_last_bucket = true;
    }
        
    template<typename... Args>
    void set_value_of_empty_bucket(distance_type dist_from_initial_bucket, 
                                   truncated_hash_type hash, Args&&... value_type_args) 
    {
        tsl_assert(dist_from_initial_bucket >= 0);
        tsl_assert(empty());
        
        ::new (static_cast<void*>(std::addressof(m_value))) value_type(std::forward<Args>(value_type_args)...);
        this->set_hash(hash);
        m_dist_from_initial_bucket = dist_from_initial_bucket;
        
        tsl_assert(!empty());
    }
    
    void swap_with_value_in_bucket(distance_type& dist_from_init_bucket, 
                                   truncated_hash_type& hash, value_type& value) 
    {
        tsl_assert(!empty());
        
        using std::swap;
        swap(value, this->value());
        swap(dist_from_init_bucket, m_dist_from_initial_bucket);
        
        (void) hash;
        if(StoreHash) {
            truncated_hash_type this_hash = this->truncated_hash();
            this->set_hash(hash);
            hash = this_hash;
        }
    }
    
    static truncated_hash_type truncate_hash(std::size_t hash) {
        return truncated_hash_type(hash);
    }
    
private:
    void destroy_value() noexcept {
        try {
            tsl_assert(!empty());
            
            value().~value_type();
        }
        catch(...) {
            std::terminate();
        }
    }
    
private:
    using storage = typename std::aligned_storage<sizeof(value_type), alignof(value_type)>::type;
    
    static const distance_type EMPTY_MARKER_DIST_FROM_INIT_BUCKET = -1;
    
    distance_type m_dist_from_initial_bucket;
    bool m_last_bucket;
    storage m_value;
};


template<typename U>
struct is_power_of_two_policy: std::false_type {};

template<std::size_t GrowthFactor>
struct is_power_of_two_policy<tsl::power_of_two_growth_policy_rh<GrowthFactor>>: std::true_type {};


/**
 * Internal common class used by robin_map and robin_set. 
 * 
 * ValueType is what will be stored by robin_hash (usually std::pair<Key, T> for map and Key for set).
 * 
 * KeySelect should be a FunctionObject which takes a ValueType in parameter and returns a reference to the key.
 * 
 * ValueSelect should be a FunctionObject which takes a ValueType in parameter and returns a reference to the value.
 * ValueSelect should be void if there is no value (in a set for example).
 * 
 * The strong exception guarantee only holds if the expression 
 * 'std::is_nothrow_swappable<ValueType>::value && std::is_nothrow_move_constructible<ValueType>::value' is true.
 * 
 * Behaviour is undefined if the destructor of ValueType throws.
 */
template<class ValueType,
         class KeySelect,
         class ValueSelect,
         class Hash,
         class KeyEqual,
         class Allocator,
         bool StoreHash,
         class GrowthPolicy>
class robin_hash: private Hash, private KeyEqual, private GrowthPolicy {
private:    
    template<typename U>
    using has_mapped_type = typename std::integral_constant<bool, !std::is_same<U, void>::value>;
    

public:
    template<bool is_const>
    class robin_iterator;
    
    using key_type = typename KeySelect::key_type;
    using value_type = ValueType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = robin_iterator<false>;
    using const_iterator = robin_iterator<true>;
    
    
private:
    /**
     * Either store hash because we are asked by the StoreHash template parameter
     * or store the hash because it doesn't cost us anything in size
     * and can be used to speed up rehash.
     */
    static constexpr bool STORE_HASH = StoreHash || 
                                       (
                                         (sizeof(tsl::detail_robin_hash::bucket_entry<value_type, true>) ==
                                          sizeof(tsl::detail_robin_hash::bucket_entry<value_type, false>))
                                         &&
                                         (sizeof(std::size_t) == sizeof(truncated_hash_type) ||
                                          is_power_of_two_policy<GrowthPolicy>::value)
                                         &&
                                          // Don't store the hash for primitive types with default hash.
                                          (!std::is_arithmetic<key_type>::value || !std::is_same<Hash, std::hash<key_type>>::value)
                                       );
                                        
    /**
     * Only use the stored hash on lookup if we are explictly asked. We are not sure how slow
     * the KeyEqual operation is. An extra comparison may slow things down with a fast KeyEqual.
     */
    static constexpr bool USE_STORED_HASH_ON_LOOKUP = StoreHash;

    /**
     * We can only use the hash on rehash if the size of the hash type is the same as the stored one or
     * if we use a power of two modulo. In the case of the power of two modulo, we just mask
     * the least significant bytes, we just have to check that the truncated_hash_type didn't truncated
     * more bytes.
     */
    static bool USE_STORED_HASH_ON_REHASH(size_type bucket_count) {
        (void) bucket_count;
        if(STORE_HASH && sizeof(std::size_t) == sizeof(truncated_hash_type)) {
            return true;
        }
        else if(STORE_HASH && is_power_of_two_policy<GrowthPolicy>::value) {
            tsl_assert(bucket_count > 0);
            return (bucket_count - 1) <= std::numeric_limits<truncated_hash_type>::max();
        }
        else {
            return false;   
        }
    }
    
    using bucket_entry = tsl::detail_robin_hash::bucket_entry<value_type, STORE_HASH>;
    using distance_type = typename bucket_entry::distance_type;
    
    using buckets_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<bucket_entry>;
    using buckets_container_type = std::vector<bucket_entry, buckets_allocator>;
    
    
public:
    template<bool is_const>
    class robin_iterator {
        friend class robin_hash;
        
    private:
        using iterator_bucket = typename std::conditional<is_const, 
                                                          typename buckets_container_type::const_iterator, 
                                                          typename buckets_container_type::iterator>::type;
    
        
        robin_iterator(iterator_bucket it) noexcept: m_iterator(it) {
        }
        
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const typename robin_hash::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using pointer = value_type*;
        
        
        robin_iterator() noexcept {
        }
        
        robin_iterator(const robin_iterator<false>& other) noexcept: m_iterator(other.m_iterator) {
        }
        
        const typename robin_hash::key_type& key() const {
            return KeySelect()(m_iterator->value());
        }

        template<class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value && is_const>::type* = nullptr>
        const typename U::value_type& value() const {
            return U()(m_iterator->value());
        }

        template<class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value && !is_const>::type* = nullptr>
        typename U::value_type& value() {
            return U()(m_iterator->value());
        }
        
        reference operator*() const {
            return m_iterator->value();
        }
        
        pointer operator->() const {
            return std::addressof(m_iterator->value());
        }
        
        robin_iterator& operator++() {
            while(true) {
                if(m_iterator->last_bucket()) {
                    ++m_iterator;
                    return *this;
                }
                
                ++m_iterator;
                if(!m_iterator->empty()) {
                    return *this;
                }
            }
        }
        
        robin_iterator operator++(int) {
            robin_iterator tmp(*this);
            ++*this;
            
            return tmp;
        }
        
        friend bool operator==(const robin_iterator& lhs, const robin_iterator& rhs) { 
            return lhs.m_iterator == rhs.m_iterator; 
        }
        
        friend bool operator!=(const robin_iterator& lhs, const robin_iterator& rhs) { 
            return !(lhs == rhs); 
        }
        
    protected:
        typename robin_hash::value_type& key_value_ref() noexcept {
            return m_iterator->value();
        }
        
    private:
        iterator_bucket m_iterator;
    };

    
public:
    robin_hash(size_type bucket_count, 
               const Hash& hash,
               const KeyEqual& equal,
               const Allocator& alloc,
               float max_load_factor): Hash(hash), KeyEqual(equal),
                                       // We need a non-zero bucket_count
                                       GrowthPolicy(bucket_count == 0?++bucket_count:bucket_count),
                                       m_buckets(alloc), 
                                       m_bucket_count(bucket_count),
                                       m_nb_elements(0), 
                                       m_grow_on_next_insert(false)
    {
        if(bucket_count > max_bucket_count()) {
            throw std::length_error("The map exceeds its maxmimum size.");
        }
        
        // Can't directly construct with the appropriate size in the initializer 
        // as m_buckets(bucket_count, alloc) is not supported by GCC 4.8
        m_buckets.resize(m_bucket_count);
        
        tsl_assert(!m_buckets.empty());
        m_buckets.back().set_as_last_bucket();
        
        
        this->max_load_factor(max_load_factor);
    }
    
    robin_hash(const robin_hash& other) = default;
    
    robin_hash(robin_hash&& other) noexcept(std::is_nothrow_move_constructible<Hash>::value &&
                                            std::is_nothrow_move_constructible<KeyEqual>::value &&
                                            std::is_nothrow_move_constructible<GrowthPolicy>::value &&
                                            std::is_nothrow_move_constructible<buckets_container_type>::value)
                                          : Hash(std::move(static_cast<Hash&>(other))),
                                            KeyEqual(std::move(static_cast<KeyEqual&>(other))),
                                            GrowthPolicy(std::move(static_cast<GrowthPolicy&>(other))),
                                            m_buckets(std::move(other.m_buckets)),
                                            m_bucket_count(other.m_bucket_count),
                                            m_nb_elements(other.m_nb_elements),
                                            m_load_threshold(other.m_load_threshold),
                                            m_max_load_factor(other.m_max_load_factor),
                                            m_grow_on_next_insert(other.m_grow_on_next_insert)
    {
        other.clear();
    }
    
    robin_hash& operator=(const robin_hash& other) = default;
    
    robin_hash& operator=(robin_hash&& other) {
        other.swap(*this);
        other.clear();
        
        return *this;
    }
    
    allocator_type get_allocator() const {
        return m_buckets.get_allocator();
    }
    
    
    /*
     * Iterators
     */
    iterator begin() noexcept {
        auto begin = m_buckets.begin();
        while(begin != m_buckets.end() && begin->empty()) {
            ++begin;
        }
        
        return iterator(begin);
    }
    
    const_iterator begin() const noexcept {
        return cbegin();
    }
    
    const_iterator cbegin() const noexcept {
        auto begin = m_buckets.cbegin();
        while(begin != m_buckets.cend() && begin->empty()) {
            ++begin;
        }
        
        return const_iterator(begin);
    }
    
    iterator end() noexcept {
        return iterator(m_buckets.end());
    }
    
    const_iterator end() const noexcept {
        return cend();
    }
    
    const_iterator cend() const noexcept {
        return const_iterator(m_buckets.cend());
    }
    
    
    /*
     * Capacity
     */
    bool empty() const noexcept {
        return m_nb_elements == 0;
    }
    
    size_type size() const noexcept {
        return m_nb_elements;
    }
    
    size_type max_size() const noexcept {
        return m_buckets.max_size();
    }
    
    /*
     * Modifiers
     */
    void clear() noexcept {
        for(auto& bucket: m_buckets) {
            bucket.clear();
        }
        
        m_nb_elements = 0;
        m_grow_on_next_insert = false;
    }
    
    
    
    template<typename P>
    std::pair<iterator, bool> insert(P&& value) {
        const std::size_t hash = hash_key(KeySelect()(value));
        return insert_impl(KeySelect()(value), hash, std::forward<P>(value));
    }
    
    template<typename P>
    iterator insert(const_iterator hint, P&& value) { 
        if(hint != cend() && compare_keys(KeySelect()(*hint), KeySelect()(value))) { 
            return mutable_iterator(hint); 
        }
        
        return insert(std::forward<P>(value)).first; 
    }
    
    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        if(std::is_base_of<std::forward_iterator_tag, 
                           typename std::iterator_traits<InputIt>::iterator_category>::value) 
        {
            const auto nb_elements_insert = std::distance(first, last);
            const size_type nb_free_buckets = m_load_threshold - size();
            tsl_assert(m_load_threshold >= size());
            
            if(nb_elements_insert > 0 && nb_free_buckets < size_type(nb_elements_insert)) {
                reserve(size() + size_type(nb_elements_insert));
            }
        }
        
        for(; first != last; ++first) {
            insert(*first);
        }
    }
    
    
    
    template<class K, class M>
    std::pair<iterator, bool> insert_or_assign(K&& key, M&& obj) { 
        auto it = try_emplace(std::forward<K>(key), std::forward<M>(obj));
        if(!it.second) {
            it.first.value() = std::forward<M>(obj);
        }
        
        return it;
    }
    
    template<class K, class M>
    iterator insert_or_assign(const_iterator hint, K&& key, M&& obj) {
        if(hint != cend() && compare_keys(KeySelect()(*hint), key)) { 
            auto it = mutable_iterator(hint); 
            it.value() = std::forward<M>(obj);
            
            return it;
        }
        
        return insert_or_assign(std::forward<K>(key), std::forward<M>(obj)).first;
    }

    
    template<class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return insert(value_type(std::forward<Args>(args)...));
    }
    
    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args&&... args) {
        return insert(hint, value_type(std::forward<Args>(args)...));        
    }
    
    
    
    template<class K, class... Args>
    std::pair<iterator, bool> try_emplace(K&& key, Args&&... args) { 
        const std::size_t hash = hash_key(key);
        
        return insert_impl(key, hash, std::piecewise_construct, 
                                      std::forward_as_tuple(std::forward<K>(key)), 
                                      std::forward_as_tuple(std::forward<Args>(args)...));
    }
    
    template<class K, class... Args>
    iterator try_emplace(const_iterator hint, K&& key, Args&&... args) { 
        if(hint != cend() && compare_keys(KeySelect()(*hint), key)) { 
            return mutable_iterator(hint); 
        }
        
        return try_emplace(std::forward<K>(key), std::forward<Args>(args)...).first;
    }
    
    
    iterator erase(iterator pos) {
        erase_from_bucket(pos);
        
        /**
         * Erase bucket used a backward shift after clearing the bucket.
         * Check if there is a new value in the bucket, if not get the next non-empty.
         */
        if(pos.m_iterator->empty()) {
            ++pos;
        }
        
        return pos;
    }
    
    iterator erase(const_iterator pos) {
        return erase(mutable_iterator(pos));
    }
    
    iterator erase(const_iterator first, const_iterator last) {
        if(first == last) {
            return mutable_iterator(first);
        }
        
        auto to_delete = erase(first);
        while(to_delete != last) {
            to_delete = erase(to_delete);
        }
        
        return to_delete;
    }
    
    
    template<class K>
    size_type erase(const K& key) {
        return erase(key, hash_key(key));
    }
    
    template<class K>
    size_type erase(const K& key, std::size_t hash) {
        auto it = find(key, hash);
        if(it != end()) {
            erase_from_bucket(it);
            
            return 1;
        }
        else {
            return 0;
        }
    }
    
    
    
    
    
    void swap(robin_hash& other) {
        using std::swap;
        
        swap(static_cast<Hash&>(*this), static_cast<Hash&>(other));
        swap(static_cast<KeyEqual&>(*this), static_cast<KeyEqual&>(other));
        swap(static_cast<GrowthPolicy&>(*this), static_cast<GrowthPolicy&>(other));
        swap(m_buckets, other.m_buckets);
        swap(m_bucket_count, other.m_bucket_count);
        swap(m_nb_elements, other.m_nb_elements);
        swap(m_load_threshold, other.m_load_threshold);
        swap(m_max_load_factor, other.m_max_load_factor);
        swap(m_grow_on_next_insert, other.m_grow_on_next_insert);
    }
    
    
    /*
     * Lookup
     */
    template<class K, class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value>::type* = nullptr>
    typename U::value_type& at(const K& key) {
        return at(key, hash_key(key));
    }
    
    template<class K, class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value>::type* = nullptr>
    typename U::value_type& at(const K& key, std::size_t hash) {
        return const_cast<typename U::value_type&>(static_cast<const robin_hash*>(this)->at(key, hash));
    }
    
    
    template<class K, class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value>::type* = nullptr>
    const typename U::value_type& at(const K& key) const {
        return at(key, hash_key(key));
    }
    
    template<class K, class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value>::type* = nullptr>
    const typename U::value_type& at(const K& key, std::size_t hash) const {
        auto it = find(key, hash);
        if(it != cend()) {
            return it.value();
        }
        else {
            throw std::out_of_range("Couldn't find key.");
        }
    }
    
    template<class K, class U = ValueSelect, typename std::enable_if<has_mapped_type<U>::value>::type* = nullptr>
    typename U::value_type& operator[](K&& key) {
        return try_emplace(std::forward<K>(key)).first.value();
    }
    
    
    template<class K>
    size_type count(const K& key) const {
        return count(key, hash_key(key));
    }
    
    template<class K>
    size_type count(const K& key, std::size_t hash) const {
        if(find(key, hash) != cend()) {
            return 1;
        }
        else {
            return 0;
        }
    }
    
    
    template<class K>
    iterator find(const K& key) {
        return find_impl(key, hash_key(key));
    }
    
    template<class K>
    iterator find(const K& key, std::size_t hash) {
        return find_impl(key, hash);
    }
    
    
    template<class K>
    const_iterator find(const K& key) const {
        return find_impl(key, hash_key(key));
    }
    
    template<class K>
    const_iterator find(const K& key, std::size_t hash) const {
        return find_impl(key, hash);
    }
    
    
    template<class K>
    std::pair<iterator, iterator> equal_range(const K& key) {
        return equal_range(key, hash_key(key));
    }
    
    template<class K>
    std::pair<iterator, iterator> equal_range(const K& key, std::size_t hash) {
        iterator it = find(key, hash);
        return std::make_pair(it, (it == end())?it:std::next(it));
    }
    
    
    template<class K>
    std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
        return equal_range(key, hash_key(key));
    }
    
    template<class K>
    std::pair<const_iterator, const_iterator> equal_range(const K& key, std::size_t hash) const {
        const_iterator it = find(key, hash);
        return std::make_pair(it, (it == cend())?it:std::next(it));
    }
    
    /*
     * Bucket interface 
     */
    size_type bucket_count() const {
        return m_bucket_count; 
    }
    
    size_type max_bucket_count() const {
        return std::min(GrowthPolicy::max_bucket_count(), m_buckets.max_size());
    }
    
    /*
     *  Hash policy 
     */
    float load_factor() const {
        return float(m_nb_elements)/float(bucket_count());
    }
    
    float max_load_factor() const {
        return m_max_load_factor;
    }
    
    void max_load_factor(float ml) {
        m_max_load_factor = ml;
        m_load_threshold = size_type(float(bucket_count())*m_max_load_factor);
    }
    
    void rehash(size_type count) {
        count = std::max(count, size_type(std::ceil(float(size())/max_load_factor())));
        rehash_impl(count);
    }
    
    void reserve(size_type count) {
        rehash(size_type(std::ceil(float(count)/max_load_factor())));
    }    
    
    /*
     * Observers
     */
    hasher hash_function() const {
        return static_cast<Hash>(*this);
    }
    
    key_equal key_eq() const {
        return static_cast<KeyEqual>(*this);
    }
    
    
    /*
     * Other
     */    
    iterator mutable_iterator(const_iterator pos) {
        return iterator(m_buckets.begin() + std::distance(m_buckets.cbegin(), pos.m_iterator));
    }
    
private:
    template<class K>
    std::size_t hash_key(const K& key) const {
        return Hash::operator()(key);
    }
    
    template<class K1, class K2>
    bool compare_keys(const K1& key1, const K2& key2) const {
        return KeyEqual::operator()(key1, key2);
    }
    
    std::size_t bucket_for_hash(std::size_t hash) const {
        return GrowthPolicy::bucket_for_hash(hash);
    }
    
    template<class U = GrowthPolicy, typename std::enable_if<is_power_of_two_policy<U>::value>::type* = nullptr>
    std::size_t next_bucket(std::size_t index) const noexcept {
        tsl_assert(index < bucket_count());
        
        return (index + 1) & this->m_mask;
    }
    
    template<class U = GrowthPolicy, typename std::enable_if<!is_power_of_two_policy<U>::value>::type* = nullptr>
    std::size_t next_bucket(std::size_t index) const noexcept {
        tsl_assert(index < bucket_count());
        
        index++;
        return (index != bucket_count())?index:0;
    }
    
    
    
    template<class K>
    iterator find_impl(const K& key, std::size_t hash) {
        return mutable_iterator(static_cast<const robin_hash*>(this)->find(key, hash));
    }
    
    template<class K>
    const_iterator find_impl(const K& key, std::size_t hash) const {
        std::size_t ibucket = bucket_for_hash(hash); 
        distance_type dist_from_init_bucket = 0;
        
        while(dist_from_init_bucket <= m_buckets[ibucket].dist_from_init_bucket()) {
            if((!USE_STORED_HASH_ON_LOOKUP || m_buckets[ibucket].bucket_hash_equal(hash)) && 
               compare_keys(KeySelect()(m_buckets[ibucket].value()), key)) 
            {
                return const_iterator(m_buckets.begin() + ibucket);
            }
            
            ibucket = next_bucket(ibucket);
            dist_from_init_bucket++;
        }
        
        return cend();
    }
    
    void erase_from_bucket(iterator pos) {
        pos.m_iterator->clear();
        m_nb_elements--;
        
        std::size_t previous_ibucket = std::size_t(std::distance(m_buckets.begin(), pos.m_iterator));
        std::size_t ibucket = next_bucket(previous_ibucket);
        
        while(m_buckets[ibucket].dist_from_init_bucket() > 0) {
            tsl_assert(m_buckets[previous_ibucket].empty());
            
            const distance_type new_distance = m_buckets[ibucket].dist_from_init_bucket() - 1;
            m_buckets[previous_ibucket].set_value_of_empty_bucket(new_distance, m_buckets[ibucket].truncated_hash(), 
                                                                  std::move(m_buckets[ibucket].value()));
            m_buckets[ibucket].clear();

            previous_ibucket = ibucket;
            ibucket = next_bucket(ibucket);
        }
    }
    
    template<class K, class... Args>
    std::pair<iterator, bool> insert_impl(const K& key, std::size_t hash, Args&&... value_type_args) {
        std::size_t ibucket = bucket_for_hash(hash); 
        distance_type dist_from_init_bucket = 0;
        
        while(dist_from_init_bucket <= m_buckets[ibucket].dist_from_init_bucket()) {
            if((!USE_STORED_HASH_ON_LOOKUP || m_buckets[ibucket].bucket_hash_equal(hash)) &&
               compare_keys(KeySelect()(m_buckets[ibucket].value()), key)) 
            {
                return std::make_pair(iterator(m_buckets.begin() + ibucket), false);
            }
            
            ibucket = next_bucket(ibucket);
            dist_from_init_bucket++;
        }
        
        if(grow_on_high_load()) {
            ibucket = bucket_for_hash(hash);
            dist_from_init_bucket = 0;
            
            while(dist_from_init_bucket <= m_buckets[ibucket].dist_from_init_bucket()) {
                ibucket = next_bucket(ibucket);
                dist_from_init_bucket++;
            }
        }
 
        
        if(m_buckets[ibucket].empty()) {
            m_buckets[ibucket].set_value_of_empty_bucket(dist_from_init_bucket, bucket_entry::truncate_hash(hash),
                                                         std::forward<Args>(value_type_args)...);
        }
        else {
            insert_value(ibucket, dist_from_init_bucket, bucket_entry::truncate_hash(hash), 
                         std::forward<Args>(value_type_args)...);
        }
        
        
        m_nb_elements++;
        /*
         * The value will be inserted in ibucket in any case, either because it was
         * empty or by stealing the bucket (robin hood). 
         */
        return std::make_pair(iterator(m_buckets.begin() + ibucket), true);
    }
    
    
    template<class... Args>
    void insert_value(std::size_t ibucket, distance_type dist_from_init_bucket, 
                      truncated_hash_type hash, Args&&... value_type_args) 
    {
        insert_value(ibucket, dist_from_init_bucket, hash, value_type(std::forward<Args>(value_type_args)...));
    }

    void insert_value(std::size_t ibucket, distance_type dist_from_init_bucket, 
                      truncated_hash_type hash, value_type&& value) 
    {
        m_buckets[ibucket].swap_with_value_in_bucket(dist_from_init_bucket, hash, value);
        ibucket = next_bucket(ibucket);
        dist_from_init_bucket++;
        
        while(!m_buckets[ibucket].empty()) {
            if(dist_from_init_bucket > m_buckets[ibucket].dist_from_init_bucket()) {
                if(dist_from_init_bucket >= REHASH_ON_HIGH_NB_PROBES__NPROBES &&
                   load_factor() >= REHASH_ON_HIGH_NB_PROBES__MIN_LOAD_FACTOR) 
                {
                    /**
                        * The number of probes is really high, rehash the map on the next insert.
                        * Difficult to do now as rehash may throw.
                        */
                    m_grow_on_next_insert = true;
                }
            
                m_buckets[ibucket].swap_with_value_in_bucket(dist_from_init_bucket, hash, value);
            }
            
            ibucket = next_bucket(ibucket);
            dist_from_init_bucket++;
        }
        
        m_buckets[ibucket].set_value_of_empty_bucket(dist_from_init_bucket, hash, std::move(value));
    }
    
    
    void rehash_impl(size_type count) {
        robin_hash new_table(count, static_cast<Hash&>(*this), static_cast<KeyEqual&>(*this), 
                             get_allocator(), m_max_load_factor);
        
        const bool use_stored_hash = USE_STORED_HASH_ON_REHASH(new_table.bucket_count());
        for(auto& bucket: m_buckets) {
            if(bucket.empty()) { 
                continue; 
            }
            
            const std::size_t hash = use_stored_hash?bucket.truncated_hash():
                                                     new_table.hash_key(KeySelect()(bucket.value()));
                                                     
            new_table.insert_value_on_rehash(new_table.bucket_for_hash(hash), 0, 
                                             bucket_entry::truncate_hash(hash), std::move(bucket.value()));
        }
        
        new_table.m_nb_elements = m_nb_elements;
        new_table.swap(*this);
    }
    
    void insert_value_on_rehash(std::size_t ibucket, distance_type dist_from_init_bucket, 
                                truncated_hash_type hash, value_type&& value) 
    {
        while(true) {
            if(dist_from_init_bucket > m_buckets[ibucket].dist_from_init_bucket()) {
                if(m_buckets[ibucket].empty()) {
                    m_buckets[ibucket].set_value_of_empty_bucket(dist_from_init_bucket, hash, std::move(value));
                    return;
                }
                else {
                    m_buckets[ibucket].swap_with_value_in_bucket(dist_from_init_bucket, hash, value);
                }
            }
            
            dist_from_init_bucket++;
            ibucket = next_bucket(ibucket);
        }
    }
    
    
    
    /**
     * Return true if the map has been rehashed
     */
    bool grow_on_high_load() {
        if(m_grow_on_next_insert || size() >= m_load_threshold) {
            rehash_impl(GrowthPolicy::next_bucket_count());
            m_grow_on_next_insert = false;
            
            return true;
        }
        
        return false;
    }

    
public:
    static const size_type DEFAULT_INIT_BUCKETS_SIZE = 16;
    static constexpr float DEFAULT_MAX_LOAD_FACTOR = 0.5f;
    
private:
    static const distance_type REHASH_ON_HIGH_NB_PROBES__NPROBES = 128;
    static constexpr float REHASH_ON_HIGH_NB_PROBES__MIN_LOAD_FACTOR = 0.15f;
    
private:
    buckets_container_type m_buckets;
    
    /**
     * Used a lot in find, avoid the call to m_buckets.size() which is a bit slower.
     */
    size_type m_bucket_count;
    
    size_type m_nb_elements;
    
    size_type m_load_threshold;
    float m_max_load_factor;
    
    bool m_grow_on_next_insert;
};

}

}

#endif
