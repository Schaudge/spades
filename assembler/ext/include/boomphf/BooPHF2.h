// BooPHF library
// intended to be a minimal perfect hash function with fast and low memory
// construction, at the cost of (slightly) higher bits/elem than other state of
// the art libraries once built.  should work with arbitray large number of
// elements, based on a cascade of "collision-free" bit arrays

#pragma once
#include <cstdio>
#include <climits>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <cstring>

#include <array>
#include <unordered_map>
#include <vector>
#include <memory> // for make_shared
#include <unistd.h>

namespace boomphf {

inline uint64_t printPt( pthread_t pt) {
    unsigned char *ptc = (unsigned char*)(void*)(&pt);
    uint64_t res =0;
    for (size_t i=0; i<sizeof(pt); i++) {
        res+= (unsigned)(ptc[i]);
    }
    return res;
}


////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark utils
////////////////////////////////////////////////////////////////

#define L8 0x0101010101010101ULL // Every lowest 8th bit set: 00000001...
#define G2 0xAAAAAAAAAAAAAAAAULL // Every highest 2nd bit: 101010...
#define G4 0x3333333333333333ULL // 00110011 ... used to group the sum of 4 bits.
#define G8 0x0F0F0F0F0F0F0F0FULL

static inline unsigned popcount_64(uint64_t x) {
    // Step 1:  00 - 00 = 0;  01 - 00 = 01; 10 - 01 = 01; 11 - 01 = 10;
    x = x - ((x & G2) >> 1);
    // step 2:  add 2 groups of 2.
    x = (x & G4) + ((x >> 2) & G4);
    // 2 groups of 4.
    x = (x + (x >> 4)) & G8;
    // Using a multiply to collect the 8 groups of 8 together.
    x = x * L8 >> 56;
    return x;
}

////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark hasher
////////////////////////////////////////////////////////////////

typedef std::array<uint64_t,2> hash_pair_t;
typedef hash_pair_t internal_hash_t; // ou hash_pair_t directement ?  __uint128_t
typedef std::vector<internal_hash_t>::const_iterator vectorit_hash128_t;

struct internalHasher {
    uint64_t operator()(const internal_hash_t& key) const {
        uint64_t s0 = key[0];
        uint64_t s1 = key[1];
        s1 ^= s1 << 23;
        return  (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26)) + s0;
    }
};

template<class SingleHasher_t> class XorshiftHashFunctors {
    /*  Xorshift128*
        Written in 2014 by Sebastiano Vigna (vigna@acm.org)

        To the extent possible under law, the author has dedicated all copyright
        and related and neighboring rights to this software to the public domain
        worldwide. This software is distributed without any warranty.

        See <http://creativecommons.org/publicdomain/zero/1.0/>.

        This is the fastest generator passing BigCrush without
        systematic failures, but due to the relatively short period it is
        acceptable only for applications with a mild amount of parallelism;
        otherwise, use a xorshift1024* generator.

        The state must be seeded so that it is not everywhere zero. If you have
        a nonzero 64-bit seed, we suggest to pass it twice through
        MurmurHash3's avalanching function. */
  public:
    template<class Item>
    hash_pair_t hashpair128(const Item& key) const {
        auto h = singleHasher(key);
        return { h.first, h.second };
    }

    //return next hash an update state s
    uint64_t next(hash_pair_t &s) const {
        uint64_t s1 = s[0];
        const uint64_t s0 = s[1];
        s[0] = s0;
        s1 ^= s1 << 23; // a
        return (s[1] = (s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26))) + s0; // b, c
    }

  private:
    SingleHasher_t singleHasher;
};


////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark iterators
////////////////////////////////////////////////////////////////

template <typename Iterator>
struct iter_range {
    iter_range(Iterator b, Iterator e)
            : m_begin(std::move(b)), m_end(std::move(e)) {}

    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }

    Iterator m_begin, m_end;
};

template <typename Iterator>
iter_range<Iterator> range(Iterator begin, Iterator end) {
    return iter_range<Iterator>(std::move(begin), std::move(end));
}

////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark BitVector
////////////////////////////////////////////////////////////////

class bitVector {

  public:

    bitVector() : _size(0)
    {
        _bitArray = nullptr;
    }

    bitVector(uint64_t n)
            : _size(n) {
        _nchar  = (1ULL+n/64ULL);
        _bitArray =  (uint64_t *) calloc(_nchar,sizeof(uint64_t));
    }

    ~bitVector() {
        if (_bitArray != nullptr)
            free(_bitArray);
    }

    //copy constructor
    bitVector(bitVector const &r) {
        _size =  r._size;
        _nchar = r._nchar;
        _ranks = r._ranks;
        _bitArray = nullptr;
        if (r._bitArray) {
            _bitArray = (uint64_t *) calloc(_nchar,sizeof(uint64_t));
            memcpy(_bitArray, r._bitArray, _nchar*sizeof(uint64_t) );
        }
    }

    // Copy assignment operator
    bitVector &operator=(bitVector const &r)
    {
        if (&r != this) {
            _size =  r._size;
            _nchar = r._nchar;
            _ranks = r._ranks;
            if (_bitArray != nullptr)
                free(_bitArray);
            _bitArray = (uint64_t *) calloc(_nchar, sizeof(uint64_t));
            memcpy(_bitArray, r._bitArray, _nchar*sizeof(uint64_t) );
        }
        return *this;
    }

    // Move assignment operator
    bitVector &operator=(bitVector &&r) noexcept {
        //printf("bitVector move assignment \n");
        if (&r != this)
        {
            if(_bitArray != nullptr)
                free(_bitArray);

            _size =  std::move (r._size);
            _nchar = std::move (r._nchar);
            _ranks = std::move (r._ranks);
            _bitArray = r._bitArray;
            r._bitArray = nullptr;
        }
        return *this;
    }
    // Move constructor
    bitVector(bitVector &&r) : _bitArray ( nullptr),_size(0)
    {
        *this = std::move(r);
    }


    void resize(uint64_t newsize)
    {
        //printf("bitvector resize from  %llu bits to %llu \n",_size,newsize);
        _nchar  = (1ULL+newsize/64ULL);
        _bitArray = (uint64_t *) realloc(_bitArray,_nchar*sizeof(uint64_t));
        _size = newsize;
    }

    size_t size() const
    {
        return _size;
    }

    uint64_t bitSize() const {return (_nchar*64ULL + _ranks.capacity()*64ULL );}

    //clear whole array
    void clear()
    {
        memset(_bitArray,0,_nchar*sizeof(uint64_t));
    }

    //clear collisions in interval, only works with start and size multiple of 64
    void clearCollisions(uint64_t start, size_t size, bitVector * cc)
    {
        assert( (start & 63) ==0);
        assert( (size & 63) ==0);
        uint64_t ids = (start/64ULL);
        for(uint64_t ii =0;  ii< (size/64ULL); ii++ )
        {
            _bitArray[ids+ii] =  _bitArray[ids+ii] & (~ (cc->get64(ii)) );
        }

        cc->clear();
    }


    //clear interval, only works with start and size multiple of 64
    void clear(uint64_t start, size_t size)
    {
        assert( (start & 63) ==0);
        assert( (size & 63) ==0);
        memset(_bitArray + (start/64ULL),0,(size/64ULL)*sizeof(uint64_t));
    }

    //for debug purposes
    void print() const
    {
        printf("bit array of size %lli: \n",_size);
        for(uint64_t ii = 0; ii< _size; ii++)
        {
            if(ii%10==0)
                printf(" (%llu) ",ii);
            int val = (_bitArray[ii >> 6] >> (ii & 63 ) ) & 1;
            printf("%i",val);
        }
        printf("\n");

        printf("rank array : size %lu \n",_ranks.size());
        for (uint64_t ii = 0; ii< _ranks.size(); ii++)
        {
            printf("%llu :  %lli,  ",ii,_ranks[ii]);
        }
        printf("\n");
    }

    // return value at pos
    uint64_t operator[](uint64_t pos) const {
        //unsigned char * _bitArray8 = (unsigned char *) _bitArray;
        //return (_bitArray8[pos >> 3ULL] >> (pos & 7 ) ) & 1;
        return (_bitArray[pos >> 6ULL] >> (pos & 63)) & 1;
    }

    //atomically   return old val and set to 1
    uint64_t atomic_test_and_set(uint64_t pos) {
        uint64_t oldval =   __sync_fetch_and_or(_bitArray + (pos >> 6), (uint64_t) (1ULL << (pos & 63)) );
        return (oldval >> (pos & 63)) & 1;
    }


    uint64_t get(uint64_t pos) const {
        return (*this)[pos];
    }

    uint64_t get64(uint64_t cell64) const {
        return _bitArray[cell64];
    }

    //set bit pos to 1
    void set(uint64_t pos) {
        assert(pos<_size);
        //_bitArray [pos >> 6] |=   (1ULL << (pos & 63) ) ;
        __sync_fetch_and_or (_bitArray + (pos >> 6ULL), (1ULL << (pos & 63)) );
    }

    //set bit pos to 0
    void reset(uint64_t pos) {
        //_bitArray [pos >> 6] &=   ~(1ULL << (pos & 63) ) ;
        __sync_fetch_and_and (_bitArray + (pos >> 6ULL), ~(1ULL << (pos & 63) ));
    }

    // return value of last rank
    // add offset to all ranks computed
    uint64_t build_ranks(uint64_t offset = 0) {
        _ranks.reserve(2 + _size/_nb_bits_per_rank_sample);

        uint64_t curent_rank = offset;
        for (size_t ii = 0; ii < _nchar; ii++) {
            if (((ii*64)  % _nb_bits_per_rank_sample) == 0) {
                _ranks.push_back(curent_rank);
            }
            curent_rank +=  popcount_64(_bitArray[ii]);
        }

        return curent_rank;
    }

    uint64_t rank(uint64_t pos) const {
        uint64_t word_idx = pos / 64ULL;
        uint64_t word_offset = pos % 64;
        uint64_t block = pos / _nb_bits_per_rank_sample;
        uint64_t r = _ranks[block];
        for (uint64_t w = block * _nb_bits_per_rank_sample / 64; w < word_idx; ++w)
            r += popcount_64(_bitArray[w]);
        uint64_t mask = (uint64_t(1) << word_offset ) - 1;
        r += popcount_64( _bitArray[word_idx] & mask);

        return r;
    }

    void save(std::ostream& os) const {
        os.write(reinterpret_cast<char const*>(&_size), sizeof(_size));
        os.write(reinterpret_cast<char const*>(&_nchar), sizeof(_nchar));
        os.write(reinterpret_cast<char const*>(_bitArray), (std::streamsize)(sizeof(uint64_t) * _nchar));
        size_t sizer = _ranks.size();
        os.write(reinterpret_cast<char const*>(&sizer),  sizeof(size_t));
        os.write(reinterpret_cast<char const*>(_ranks.data()), (std::streamsize)(sizeof(_ranks[0]) * _ranks.size()));
    }

    void load(std::istream& is) {
        is.read(reinterpret_cast<char*>(&_size), sizeof(_size));
        is.read(reinterpret_cast<char*>(&_nchar), sizeof(_nchar));
        this->resize(_size);
        is.read(reinterpret_cast<char *>(_bitArray), (std::streamsize)(sizeof(uint64_t) * _nchar));

        size_t sizer;
        is.read(reinterpret_cast<char *>(&sizer),  sizeof(size_t));
        _ranks.resize(sizer);
        is.read(reinterpret_cast<char*>(_ranks.data()), (std::streamsize)(sizeof(_ranks[0]) * _ranks.size()));
    }


  protected:
    uint64_t*  _bitArray;
    //uint64_t* _bitArray;
    uint64_t _size;
    uint64_t _nchar;

    // epsilon =  64 / _nb_bits_per_rank_sample   bits
    // additional size for rank is epsilon * _size
    static const uint64_t _nb_bits_per_rank_sample = 512; //512 seems ok
    std::vector<uint64_t> _ranks;
};

////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark level
////////////////////////////////////////////////////////////////


static inline uint64_t fastrange64(uint64_t word, uint64_t p) {
    //return word %  p;
    return (uint64_t)(((__uint128_t)word * (__uint128_t)p) >> 64);
}

class level{
  public:
    level() {}

    ~level() {}

    uint64_t get(uint64_t hash_raw) const {
        uint64_t hashi = fastrange64(hash_raw, hash_domain);
        return bitset.get(hashi);
    }

    uint64_t hash_domain;
    bitVector bitset;
};


////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark mphf
////////////////////////////////////////////////////////////////

#define NBBUFF 10000

template<typename Range,typename Iterator>
struct thread_args {
    void * boophf;
    Range const * range;
    std::shared_ptr<void> it_p; /* used to be "Iterator it" but because of fastmode, iterator is polymorphic; TODO: think about whether it should be a unique_ptr actually */
    std::shared_ptr<void> until_p; /* to cache the "until" variable */
    int level;
};

//forward declaration

template <typename Hasher_t, typename Range, typename it_type>
void * thread_processLevel(void * args);

/* Hasher_t returns a single hash when operator()(elem_t key) is called.
   if used with XorshiftHashFunctors, it must have the following operator: operator()(elem_t key, uint64_t seed) */
template<typename Hasher_t>
class mphf {
    /* this mechanisms gets P hashes out of Hasher_t */
    typedef XorshiftHashFunctors<Hasher_t> MultiHasher_t ;

  public:
    mphf()
            : _built(false) {}

    ~mphf() {}

    // allow perc_elem_loaded  elements to be loaded in ram for faster construction (default 3%), set to 0 to desactivate
    mphf(size_t n, 
         double gamma = 2.0, float perc_elem_loaded = 0.03f)
            : _nb_levels(0), _gamma(gamma), _hash_domain(size_t(ceil(double(n) * gamma))),
              _nelem(n), _percent_elem_loaded_for_fastMode(perc_elem_loaded),
              _built(false) {
        if (n ==0)
            return;

        _fastmode = _percent_elem_loaded_for_fastMode > 0.0;

        setup();
    }

    template <typename Range>
    void build(const Range &input_range,
               int num_thread = 1) {
        _num_thread = num_thread; // FIXME

        pthread_mutex_init(&_mutex, NULL);

        uint64_t offset = 0;
        for (int ii = 0; ii< _nb_levels; ii++) {
            _tempBitset =  new bitVector(_levels[ii].hash_domain); // temp collision bitarray for this level
            processLevel(input_range, ii);
            _levels[ii].bitset.clearCollisions(0 , _levels[ii].hash_domain, _tempBitset);
            offset = _levels[ii].bitset.build_ranks(offset);

            delete _tempBitset;
        }

        _lastbitsetrank = offset;
        std::vector<internal_hash_t>().swap(setLevelFastmode);   // clear setLevelFastmode reallocating

        pthread_mutex_destroy(&_mutex);

        _built = true;
    }
    
    template<class elem_t>
    uint64_t lookup(const elem_t &elem) const {
        if (!_built) return ULLONG_MAX;

        //auto hashes = _hasher(elem);
        uint64_t non_minimal_hp, minimal_hp;
        int level;

        hash_pair_t bbhash = _hasher.hashpair128(elem);
        uint64_t level_hash = getLevel(bbhash, elem, &level);

        if (level == (_nb_levels-1)) {
            auto in_final_map = _final_hash.find(bbhash);
            if (in_final_map == _final_hash.end())
                //elem was not in orignal set of keys
                return ULLONG_MAX; //  means elem not in set

            minimal_hp =  in_final_map->second + _lastbitsetrank;
            return minimal_hp;
        } else {
            non_minimal_hp = fastrange64(level_hash,_levels[level].hash_domain);
        }

        return _levels[level].bitset.rank(non_minimal_hp); // minimal_hp
    }

    uint64_t size() const {
        return _nelem;
    }

    uint64_t mem_size() const {
        uint64_t totalsizeBitset = 0;
        for (int ii = 0; ii < _nb_levels; ii++)
            totalsizeBitset += _levels[ii].bitset.bitSize();

        uint64_t totalsize = totalsizeBitset +  _final_hash.size()*42*8 ;  // unordered map takes approx 42B per elem [personal test] (42B with uint64_t key, would be larger for other type of elem)

        /*
        printf("Bitarray    %12llu  bits (%.2f %%)   (array + ranks )\n",
               totalsizeBitset, 100*(float)totalsizeBitset/totalsize);
        printf("final hash  %12lu  bits (%.2f %%) (nb in final hash %lu)\n",
               _final_hash.size()*42*8, 100*(float)(_final_hash.size()*42*8)/totalsize,
               _final_hash.size() );
        */

        return totalsize / 8;
    }

    template <typename Iterator>
    void fillBuffer(std::vector<internal_hash_t> &buffer,
                    std::shared_ptr<Iterator> shared_it, std::shared_ptr<Iterator> until_p,
                    uint64_t &inbuff, bool & isRunning) {
        auto until = *until_p;
        pthread_mutex_lock(&_mutex);
        for (; inbuff<NBBUFF && (*shared_it)!=until; ++(*shared_it)) {
            buffer[inbuff]= _hasher.hashpair128(*(*shared_it));
            inbuff++;
        }

        if ((*shared_it)==until)
            isRunning =false;
        pthread_mutex_unlock(&_mutex);

    }

    void fillBuffer(std::vector<internal_hash_t> &buffer,
                    std::shared_ptr<vectorit_hash128_t> shared_it, std::shared_ptr<vectorit_hash128_t> until_p,
                    uint64_t &inbuff, bool &isRunning) {
        fillBufferCommon128(buffer,shared_it,until_p,inbuff,isRunning);
    }

    template <typename Iterator>
    void fillBufferCommon128(std::vector<internal_hash_t> &buffer,
                             std::shared_ptr<Iterator> shared_it, std::shared_ptr<Iterator> until_p,
                             uint64_t &inbuff, bool &isRunning) {
        auto until = *until_p;
        pthread_mutex_lock(&_mutex);
        for (; inbuff<NBBUFF && (*shared_it)!=until; ++(*shared_it)) {
            buffer[inbuff]= (*(*shared_it)); inbuff++;
        }

        if ((*shared_it)==until)
            isRunning =false;
        pthread_mutex_unlock(&_mutex);
    }

    template <typename Iterator>  //typename Range,
    void pthread_processLevel(std::vector<internal_hash_t> &buffer, std::shared_ptr<Iterator> shared_it, std::shared_ptr<Iterator> until_p, int i) {
        uint64_t inbuff =0;

        uint64_t writebuff =0;
        for (bool isRunning=true;  isRunning ; ) {
            //safely copy n items into buffer
            //call to specialized function accordin to iterator type (may be iterator over keys (first 2 levels), or iterator over 128 bit hashes)
            fillBuffer(buffer, shared_it, until_p, inbuff, isRunning);

            //do work on the n elems of the buffer
            for (uint64_t ii=0; ii<inbuff ; ii++) {
                //internal_hash_t val = buffer[ii];
                internal_hash_t bbhash = buffer[ii];
                internal_hash_t val = bbhash;

                //auto hashes = _hasher(val);
                //hash_pair_t bbhash;
                int level;

                getLevel(bbhash, &level, i);

                if (level != i)
                    continue;
                
                //insert into lvl i
                if (_fastmode && i == _fastModeLevel) {
                    uint64_t idxl2 = __sync_fetch_and_add(& _idxLevelsetLevelFastmode,1);
                    //si depasse taille attendue pour setLevelFastmode, fall back sur slow mode mais devrait pas arriver si hash ok et proba avec nous
                    if (idxl2>= setLevelFastmode.size())
                        _fastmode = false;
                    else
                        setLevelFastmode[idxl2] = val; // create set for fast mode
                }

                // insert to level i+1 : either next level of the cascade or final hash if last level reached
                if (i == _nb_levels-1) { //stop cascade here, insert into exact hash
                    uint64_t hashidx =  __sync_fetch_and_add(&_hashidx, 1);
                    
                    pthread_mutex_lock(&_mutex); //see later if possible to avoid this, mais pas bcp item vont la
                    // calc rank de fin  precedent level qq part, puis init hashidx avec ce rank, direct minimal, pas besoin inser ds bitset et rank
                    
                    if (_final_hash.count(val)) { // key already in final hash
                        fprintf(stderr,"The impossible happened : collision on 128 bit hashes... please switch to safe branch, and play the lottery.");
                        fprintf(stderr,"Another more likely explanation might be that you have duplicate keys in your input.\
                                        If so, you can ignore this message, but be aware that too many duplicate keys will increase ram usage\n");
                    }
                    _final_hash[val] = hashidx;

                    pthread_mutex_unlock(&_mutex);
                } else {
                    //ils ont reach ce level
                    //insert elem into curr level on disk --> sera utilise au level+1 , (mais encore besoin filtre)

                    // computes next hash
                    uint64_t level_hash;
                    if (level == 0)
                        level_hash = bbhash[0];
                    else if (level == 1)
                        level_hash = bbhash[1];
                    else
                        level_hash = _hasher.next(bbhash);
                    insertIntoLevel(level_hash, i); //should be safe
                }
            }

            inbuff = 0;
        }
    }


    void save(std::ostream& os) const {
        os.write(reinterpret_cast<char const*>(&_gamma), sizeof(_gamma));
        os.write(reinterpret_cast<char const*>(&_nb_levels), sizeof(_nb_levels));
        os.write(reinterpret_cast<char const*>(&_lastbitsetrank), sizeof(_lastbitsetrank));
        os.write(reinterpret_cast<char const*>(&_nelem), sizeof(_nelem));
        for (int ii=0; ii<_nb_levels; ii++) {
            _levels[ii].bitset.save(os);
        }

        //save final hash
        size_t final_hash_size = _final_hash.size();

        os.write(reinterpret_cast<char const*>(&final_hash_size), sizeof(size_t));

        for (auto it = _final_hash.begin(); it != _final_hash.end(); ++it) {
            os.write(reinterpret_cast<char const*>(&(it->first)), sizeof(internal_hash_t));
            os.write(reinterpret_cast<char const*>(&(it->second)), sizeof(uint64_t));
        }

    }

    void load(std::istream& is) {
        is.read(reinterpret_cast<char*>(&_gamma), sizeof(_gamma));
        is.read(reinterpret_cast<char*>(&_nb_levels), sizeof(_nb_levels));
        is.read(reinterpret_cast<char*>(&_lastbitsetrank), sizeof(_lastbitsetrank));
        is.read(reinterpret_cast<char*>(&_nelem), sizeof(_nelem));

        _levels.resize(_nb_levels);


        for (int ii=0; ii<_nb_levels; ii++) {
            //_levels[ii].bitset = new bitVector();
            _levels[ii].bitset.load(is);
        }

        //mini setup, recompute size of each level
        _proba_collision = 1.0 -  pow(((_gamma*(double)_nelem -1 ) / (_gamma*(double)_nelem)),_nelem-1);
        uint64_t previous_idx =0;
        _hash_domain = (size_t)(ceil(double(_nelem) * _gamma)) ;
        for (int ii=0; ii<_nb_levels; ii++) {
            //_levels[ii] = new level();
            _levels[ii].hash_domain =  (( (uint64_t) (_hash_domain * pow(_proba_collision,ii)) + 63) / 64 ) * 64;
            if (_levels[ii].hash_domain == 0 )
                _levels[ii].hash_domain  = 64 ;
        }

        //restore final hash

        _final_hash.clear();
        size_t final_hash_size ;

        is.read(reinterpret_cast<char *>(&final_hash_size), sizeof(size_t));

        for (unsigned int ii=0; ii<final_hash_size; ii++) {
            internal_hash_t key;
            uint64_t value;

            is.read(reinterpret_cast<char *>(&key), sizeof(internal_hash_t));
            is.read(reinterpret_cast<char *>(&value), sizeof(uint64_t));

            _final_hash[key] = value;
        }
        _built = true;
    }


  private:
    void setup() {
        if (_fastmode)
            setLevelFastmode.resize(_percent_elem_loaded_for_fastMode * (double)_nelem );

        _proba_collision = 1.0 -  pow(((_gamma*(double)_nelem -1 ) / (_gamma*(double)_nelem)),_nelem-1);

        _nb_levels = 25; // 25
        _levels.resize(_nb_levels);

        // build levels
        for (int ii=0; ii<_nb_levels; ii++) {
            // round size to nearest superior multiple of 64, makes it easier to clear a level
            _levels[ii].hash_domain =  (( (uint64_t)(_hash_domain * pow(_proba_collision, ii)) + 63) / 64 ) * 64;
            if (_levels[ii].hash_domain == 0)
                _levels[ii].hash_domain = 64;
        }

        _fastModeLevel = _nb_levels;
        for (int ii=0; ii<_nb_levels; ii++) {
            if (pow(_proba_collision,ii) < _percent_elem_loaded_for_fastMode) {
                _fastModeLevel = ii;
                //printf("fast mode level :  %i \n",ii);
                break;
            }
        }
    }

    //overload getLevel with either elem_t or internal_hash_t
    template<class elem_t>
    uint64_t getLevel(hash_pair_t bbhash, const elem_t &val, int *res_level) const {
        int level = 0;
        uint64_t hash_raw=0;

        for (int ii = 0; ii < (_nb_levels-1) ; ii++) {
            //calc le hash suivant
            if (ii == 0)
                hash_raw = bbhash[0];
            else if (ii == 1)
                hash_raw = bbhash[1];
            else
                hash_raw = _hasher.next(bbhash);

            if (_levels[ii].get(hash_raw))
                break;

            level++;
        }

        *res_level = level;
        return hash_raw;
    }


    // compute level and returns hash of last level reached
    // FIXME: The usage of getLevel here is *super* confusing, really.
    uint64_t getLevel(internal_hash_t &bbhash, int * res_level, int maxlevel) const {
        int level = 0;
        uint64_t hash_raw=0;

        for (int ii = 0; ii<(_nb_levels-1) && ii < maxlevel; ii++) {
            //calc le hash suivant
            if (ii == 0)
                hash_raw = bbhash[0];
            else if (ii == 1)
                hash_raw = bbhash[1];
            else
                hash_raw = _hasher.next(bbhash);

            if (_levels[ii].get(hash_raw))
                break;

            level++;
        }

        *res_level = level;
        return hash_raw;
    }

    // insert into bitarray
    void insertIntoLevel(uint64_t level_hash, int i) {
        uint64_t hashl = fastrange64(level_hash, _levels[i].hash_domain);

        if (_levels[i].bitset.atomic_test_and_set(hashl))
            _tempBitset->atomic_test_and_set(hashl);
    }

    //loop to insert into level i
    template <typename Range>
    void processLevel(Range const& input_range, int i) {
        _levels[i].bitset = bitVector(_levels[i].hash_domain);
        _hashidx = 0;
        _idxLevelsetLevelFastmode = 0;

        //create  threads
        pthread_t *tab_threads= new pthread_t [_num_thread];
        typedef decltype(input_range.begin()) it_type;
        thread_args<Range, it_type> t_arg; // meme arg pour tous
        t_arg.boophf = this;
        t_arg.range = &input_range;
        t_arg.it_p = std::static_pointer_cast<void>(std::make_shared<it_type>(input_range.begin()));
        t_arg.until_p = std::static_pointer_cast<void>(std::make_shared<it_type>(input_range.end()));

        t_arg.level = i;

        if (_fastmode && i > _fastModeLevel) {
            //   we'd like to do t_arg.it = data_iterator.begin() but types are different;
            //   so, casting to (void*) because of that; and we remember the type in the template
            //	typedef decltype(setLevelFastmode.begin()) fastmode_it_type; // vectorit_hash128_t
            t_arg.it_p =  std::static_pointer_cast<void>(std::make_shared<vectorit_hash128_t>(setLevelFastmode.begin()));
            t_arg.until_p =  std::static_pointer_cast<void>(std::make_shared<vectorit_hash128_t>(setLevelFastmode.end()));

            //       we'd like to do t_arg.it = data_iterator.begin() but types are different;
            //       so, casting to (void*) because of that; and we remember the type in the template

            if (_num_thread > 1) {
                for (int ii=0;ii<_num_thread;ii++)
                    pthread_create (&tab_threads[ii], NULL,  thread_processLevel<Hasher_t, Range, vectorit_hash128_t>, &t_arg); //&t_arg[ii]
            } else {
                thread_processLevel<Hasher_t, Range, vectorit_hash128_t>(&t_arg); //&t_arg[ii]
            }
        } else {
            //printf(" _ _ basic mode \n");
            if (_num_thread > 1) {
                for(int ii=0;ii<_num_thread;ii++)
                    pthread_create (&tab_threads[ii], NULL,  thread_processLevel<Hasher_t, Range, decltype(input_range.begin())>, &t_arg); //&t_arg[ii]
            } else
                thread_processLevel<Hasher_t, Range, decltype(input_range.begin())>(&t_arg); //&t_arg[ii]
        }
        
        //joining
        if (_num_thread > 1) {
            for(int ii=0;ii<_num_thread;ii++)
            {
                pthread_join(tab_threads[ii], NULL);
            }
        }

        if (_fastmode && i == _fastModeLevel) { //shrink to actual number of elements in set
            //printf("\nresize setLevelFastmode to %lli \n",_idxLevelsetLevelFastmode);
            setLevelFastmode.resize(_idxLevelsetLevelFastmode);
        }
        delete [] tab_threads;
    }

  private:
    std::vector<level> _levels;
    int _nb_levels;
    MultiHasher_t _hasher;
    bitVector * _tempBitset;

    double _gamma;
    uint64_t _hash_domain;
    uint64_t _nelem;
    std::unordered_map<internal_hash_t,uint64_t, internalHasher> _final_hash; // internalHasher   Hasher_t
    uint64_t _hashidx;
    int _num_thread;
    double _proba_collision;
    uint64_t _lastbitsetrank;

    // fast build mode , requires  that _percent_elem_loaded_for_fastMode %   elems are loaded in ram
    float _percent_elem_loaded_for_fastMode ;
    bool _fastmode;
    uint64_t _idxLevelsetLevelFastmode;
    std::vector< internal_hash_t > setLevelFastmode;
    int _fastModeLevel;

    bool _built;

  public:  
    pthread_mutex_t _mutex;
};

////////////////////////////////////////////////////////////////
#pragma mark -
#pragma mark threading
////////////////////////////////////////////////////////////////

template<typename Hasher_t, typename Range, typename it_type>
void *thread_processLevel(void * args) {
    if (args ==NULL) return NULL;

    thread_args<Range,it_type> *targ = (thread_args<Range,it_type>*) args;
    mphf<Hasher_t> * obw = (mphf<Hasher_t > *) targ->boophf;
    int level = targ->level;
    std::vector<internal_hash_t> buffer;
    buffer.resize(NBBUFF);

    pthread_mutex_t * mutex =  & obw->_mutex;

    pthread_mutex_lock(mutex); // from comment above: "//get starting iterator for this thread, must be protected (must not be currently used by other thread to copy elems in buff)"
    std::shared_ptr<it_type> startit = std::static_pointer_cast<it_type>(targ->it_p);
    std::shared_ptr<it_type> until_p = std::static_pointer_cast<it_type>(targ->until_p);
    pthread_mutex_unlock(mutex);

    obw->pthread_processLevel(buffer, startit, until_p, level);

    return NULL;
}
}