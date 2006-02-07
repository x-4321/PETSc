#ifndef included_ALE_BiGraph_hh
#define included_ALE_BiGraph_hh

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <iostream>

// ALE extensions

#ifndef  included_Sifter_hh
#include <Sifter.hh>
#endif

namespace ALE {

  namespace Two {

    // Defines the traits of a sequence representing a subset of a multi_index container Index_.
    // A sequence defines output (input in std terminology) iterators for traversing an Index_ object.
    // Upon dereferencing values are extracted from each result record using a ValueExtractor_ object.
    template <typename Index_, typename ValueExtractor_>
    struct IndexSequenceTraits {
      typedef Index_ index_type;
      class iterator_base {
      public:
        // Standard iterator typedefs
        typedef ValueExtractor_                        extractor_type;
        typedef std::input_iterator_tag                iterator_category;
        typedef typename extractor_type::result_type   value_type;
        typedef int                                    difference_type;
        typedef value_type*                            pointer;
        typedef value_type&                            reference;
        
        // Underlying iterator type
        typedef typename index_type::iterator          itor_type;
      protected:
        // Underlying iterator 
        itor_type      _itor;
        // Member extractor
        extractor_type _ex;
      public:
        iterator_base(itor_type itor) {
          this->_itor = itor_type(itor);
        };
        virtual ~iterator_base() {};
        virtual bool              operator==(const iterator_base& iter) const {return this->_itor == iter._itor;};
        virtual bool              operator!=(const iterator_base& iter) const {return this->_itor != iter._itor;};
        // FIX: operator*() should return a const reference, but it won't compile that way, because _ex() returns const value_type
        virtual const value_type  operator*() const {return _ex(*(this->_itor));};
      };// class iterator_base
      class iterator : public iterator_base {
      public:
        // Standard iterator typedefs
        typedef typename iterator_base::iterator_category  iterator_category;
        typedef typename iterator_base::value_type         value_type;
        typedef typename iterator_base::extractor_type     extractor_type;
        typedef typename iterator_base::difference_type    difference_type;
        typedef typename iterator_base::pointer            pointer;
        typedef typename iterator_base::reference          reference;
        // Underlying iterator type
        typedef typename iterator_base::itor_type          itor_type;
      public:
        iterator(const itor_type& itor) : iterator_base(itor) {};
        virtual ~iterator() {};
        //
        virtual iterator   operator++() {++this->_itor; return *this;};
        virtual iterator   operator++(int n) {iterator tmp(this->_itor); ++this->_itor; return tmp;};
      };// class iterator
    }; // struct IndexSequenceTraits
    
    template <typename Index_, typename ValueExtractor_>
    struct ReversibleIndexSequenceTraits {
      typedef IndexSequenceTraits<Index_, ValueExtractor_> base_traits;
      typedef typename base_traits::iterator_base   iterator_base;
      typedef typename base_traits::iterator        iterator;
      typedef typename base_traits::index_type      index_type;

      // reverse_iterator is the reverse of iterator
      class reverse_iterator : public iterator_base {
      public:
        // Standard iterator typedefs
        typedef typename iterator_base::iterator_category  iterator_category;
        typedef typename iterator_base::value_type         value_type;
        typedef typename iterator_base::extractor_type     extractor_type;
        typedef typename iterator_base::difference_type    difference_type;
        typedef typename iterator_base::pointer            pointer;
        typedef typename iterator_base::reference          reference;
        // Underlying iterator type
        typedef typename iterator_base::itor_type          itor_type;
      public:
        reverse_iterator(const itor_type& itor) : iterator_base(itor) {};
        virtual ~reverse_iterator() {};
        //
        virtual reverse_iterator     operator++() {--this->_itor; return *this;};
        virtual reverse_iterator     operator++(int n) {reverse_iterator tmp(this->_itor); --this->_itor; return tmp;};
      };
    }; // class ReversibleIndexSequenceTraits


    //
    // Rec & RecContainer definitions.
    // Rec is intended to denote a graph point record.
    // 
    template <typename Point_>
    struct Rec {
      typedef Point_ point_type;
      point_type     point;
      int            degree;
      // Basic interface
      Rec() : degree(0){};
      Rec(const Rec& r) : point(r.point), degree(r.degree) {}
      Rec(const point_type& p) : point(p), degree(0) {};
      Rec(const point_type& p, const int d) : point(p), degree(d) {};
      // Printing
      friend std::ostream& operator<<(std::ostream& os, const Rec& p) {
        os << "<" << p.point << ", "<< p.degree << ">";
        return os;
      };
      
      struct degreeAdjuster {
        degreeAdjuster(int newDegree) : _newDegree(newDegree) {};
        void operator()(Rec& r) { r.degree = this->_newDegree; }
      private:
        int _newDegree;
      };// degreeAdjuster()

    };// class Rec

    template <typename Point_>
    struct RecContainerTraits {
      typedef Rec<Point_> rec_type;
      // Index tags
      struct pointTag{};
      struct degreeTag{};
      // Rec set definition
      typedef ::boost::multi_index::multi_index_container<
        rec_type,
        ::boost::multi_index::indexed_by<
          ::boost::multi_index::ordered_unique<
            ::boost::multi_index::tag<pointTag>, BOOST_MULTI_INDEX_MEMBER(rec_type, typename rec_type::point_type, point)
          >,
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<degreeTag>, BOOST_MULTI_INDEX_MEMBER(rec_type, int, degree)
          >
        >,
        ALE_ALLOCATOR<rec_type>
      > set_type; 
      //
      // Return types
      //

      class DegreeSequence {
      public:
        typedef IndexSequenceTraits<typename ::boost::multi_index::index<set_type, degreeTag>::type,
                                    BOOST_MULTI_INDEX_MEMBER(rec_type, typename rec_type::point_type,point)>
        traits;
      protected:
        const typename traits::index_type& _index;
      public:
        
        // Need to extend the inherited iterator to be able to extract the degree
        class iterator : public traits::iterator {
        public:
          iterator(const typename traits::iterator::itor_type& itor) : traits::iterator(itor) {};
          virtual const int& degree() const {return this->_itor->degree;};
        };

        DegreeSequence(const DegreeSequence& seq)           : _index(seq._index) {};
        DegreeSequence(typename traits::index_type& index)  : _index(index)     {};
        virtual ~DegreeSequence(){};

        virtual iterator begin() {
          // Retrieve the beginning iterator to the sequence of points with indegree >= 1
          return iterator(this->_index.lower_bound(1));
        };
        virtual iterator end() {
          // Retrieve the ending iterator to the sequence of points with indegree >= 1
          // Since the elements in this index are ordered by degree, this amounts to the end() of the index.
          return iterator(this->_index.end());
        };
      }; // class DegreeSequence

//      class PointSequence {
//      public:
//         typedef IndexSequenceTraits<typename ::boost::multi_index::index<set_type, pointTag>::type,
//                                     BOOST_MULTI_INDEX_MEMBER(rec_type, typename rec_type::point_type,point)>
//         traits;
//       protected:
//         const typename traits::index_type& _index;
//       public:
        
//         // Need to extend the inherited iterator to be able to extract the degree
//         class iterator : public traits::iterator {
//         public:
//           iterator(const typename traits::iterator::itor_type& itor) : traits::iterator(itor) {};
//           virtual const int& degree() const {return this->_itor->degree;};
//         };

//         DegreeSequence(const DegreeSequence& seq)           : _index(seq._index) {};
//         DegreeSequence(typename traits::index_type& index)  : _index(index)     {};
//         virtual ~DegreeSequence(){};

//         virtual iterator begin() {
//           // Retrieve the beginning iterator to the sequence of points with indegree >= 1
//           return iterator(this->_index.lower_bound(1));
//         };
//         virtual iterator end() {
//           // Retrieve the ending iterator to the sequence of points with indegree >= 1
//           // Since the elements in this index are ordered by degree, this amounts to the end() of the index.
//           return iterator(this->_index.end());
//         };
//      }; // class PointSequence
    };// struct RecContainerTraits


    template <typename Point_>
    struct RecContainer {
      typedef RecContainerTraits<Point_> traits;
      typedef typename traits::set_type set_type;
      set_type set;
      void adjustDegree(const typename traits::rec_type::point_type& p, int delta) {
        typename ::boost::multi_index::index<set_type, typename traits::pointTag>::type& index = 
          ::boost::multi_index::get<typename traits::pointTag>(this->set);
        typename ::boost::multi_index::index<set_type, typename traits::pointTag>::type::iterator i = index.find(p);
        if (i == index.end()) { // No such point exists
          if(delta < 0) { // Cannot decrease degree of a non-existent point
            ostringstream err;
            err << "ERROR: BiGraph::adjustDegree: Non-existent point " << p;
            std::cout << err << std::endl;
            throw(Exception(err.str().c_str()));
          }
          else { // We CAN INCREASE the degree of a non-existent point: simply insert a new element with degree == delta
            std::pair<typename ::boost::multi_index::index<set_type, typename traits::pointTag>::type::iterator, bool> ii;
            typename traits::rec_type r(p,delta);
            ii = index.insert(r);
            if(ii.second == false) {
              ostringstream err;
              err << "ERROR: BiGraph::adjustDegree: Failed to insert a rec " << r;
              std::cout << err << std::endl;
              throw(Exception(err.str().c_str()));
            }
          }
        }
        else { // Point exists, so we try to modify its degree
          int newDegree = i->degree + delta;
          if(newDegree < 0) {
            ostringstream ss;
            ss << "adjustDegree: Adjustment of " << *i << " by " << delta << " would result in negative degree: " << newDegree;
            throw Exception(ss.str().c_str());
          }
          if(newDegree == 0) {
            // We must erase this point
            index.erase(i);
          }
          else {
            index.modify(i, typename traits::rec_type::degreeAdjuster(newDegree));
          }
        }
      }; // adjustDegree()
    }; // struct RecContainer

    // 
    // Arrow & ArrowContainer definitions
    // 
    template<typename Source_, typename Target_, typename Color_>
    struct  Arrow { //: public ALE::def::Arrow<Source_, Target_, Color_> {
      typedef Arrow   arrow_type;
      typedef Source_ source_type;
      typedef Target_ target_type;
      typedef Color_  color_type;
      source_type source;
      target_type target;
      color_type  color;
      Arrow(const source_type& s, const target_type& t, const color_type& c) : source(s), target(t), color(c) {};

      // Printing
      friend std::ostream& operator<<(std::ostream& os, const Arrow& a) {
        os << a.source << " --" << a.color << "--> " << a.target << std::endl;
        return os;
      }

      // Arrow modifiers
      struct sourceChanger {
        sourceChanger(const source_type& newSource) : _newSource(newSource) {};
        void operator()(arrow_type& a) {a.source = this->_newSource;}
      private:
        source_type _newSource;
      };

      struct targetChanger {
        targetChanger(const target_type& newTarget) : _newTarget(newTarget) {};
        void operator()(arrow_type& a) { a.target = this->_newTarget;}
      private:
        const target_type _newTarget;
      };
    };// struct Arrow
    

    template<typename Source_, typename Target_, typename Color_>
    struct ArrowContainerTraits {
    public:
      //
      // Encapsulated types
      //
      typedef Arrow<Source_,Target_,Color_>    arrow_type;
      typedef typename arrow_type::source_type source_type;
      typedef typename arrow_type::target_type target_type;
      typedef typename arrow_type::color_type  color_type;
      // Index tags
      struct                                   sourceColorTag{};
      struct                                   targetColorTag{};
      struct                                   sourceTargetTag{};      

      // Sequence type
      template <typename Index_, typename Key_, typename SubKey_, typename ValueExtractor_>
      class ArrowSequence {
        // ArrowSequence implements ReversibleIndexSequencTraits with Index_ and ValueExtractor_ types.
        // A Key_ object and an optional SubKey_ object are used to extract the index subset.
      public:
        typedef ReversibleIndexSequenceTraits<Index_, ValueExtractor_>  traits;
        typedef Key_                                                    key_type;
        typedef SubKey_                                                 subkey_type;
      protected:
        const typename traits::index_type&                              _index;
        const key_type                                                  key;
        const subkey_type                                               subkey;
        const bool                                                      useSubkey;
      public:
        // Need to extend the inherited iterators to be able to extract arrow color
        class iterator : public traits::iterator {
        public:
          iterator(const typename traits::iterator::itor_type& itor) : traits::iterator(itor) {};
          virtual const source_type& source() const {return this->_itor->source;};
          virtual const color_type&  color()  const {return this->_itor->color;};
          virtual const target_type& target() const {return this->_itor->target;};
          virtual const arrow_type&  arrow()  const {return *(this->_itor);};
        };
        class reverse_iterator : public traits::reverse_iterator {
        public:
          reverse_iterator(const typename traits::reverse_iterator::itor_type& itor) : traits::reverse_iterator(itor) {};
          virtual const source_type& source() const {return this->_itor->source;};
          virtual const color_type&  color()  const {return this->_itor->color;};
          virtual const target_type& target() const {return this->_itor->target;};
          virtual const arrow_type&  arrow()  const {return *(this->_itor);};
        };
      public:
        //
        // Basic ArrowSequence interface
        //
        ArrowSequence(const ArrowSequence& seq) : _index(seq._index), key(seq.key), subkey(seq.subkey), useSubkey(seq.useSubkey) {};
        ArrowSequence(const typename traits::index_type& index, const key_type& k) : 
          _index(index), key(k), subkey(subkey_type()), useSubkey(0) {};
        ArrowSequence(const typename traits::index_type& index, const key_type& k, const subkey_type& kk) : 
          _index(index), key(k), subkey(kk), useSubkey(1){};
        virtual ~ArrowSequence() {};
        
        virtual iterator begin() {
          if (this->useSubkey) {
            return iterator(this->_index.lower_bound(::boost::make_tuple(this->key,this->subkey)));
          } else {
            return iterator(this->_index.lower_bound(::boost::make_tuple(this->key)));
          }
        };
        
        virtual iterator end() {
          if (this->useSubkey) {
            return iterator(this->_index.upper_bound(::boost::make_tuple(this->key,this->subkey)));
          } else {
            return iterator(this->_index.upper_bound(::boost::make_tuple(this->key)));
          }
        };
        
        virtual reverse_iterator rbegin() {
          if (this->useSubkey) {
            return reverse_iterator(--this->_index.upper_bound(::boost::make_tuple(this->key,this->subkey)));
          } else {
            return reverse_iterator(--this->_index.upper_bound(::boost::make_tuple(this->key)));
          }
        };
        
        virtual reverse_iterator rend() {
          if (this->useSubkey) {
            return reverse_iterator(--this->_index.lower_bound(::boost::make_tuple(this->key,this->subkey)));
          } else {
            return reverse_iterator(--this->_index.lower_bound(::boost::make_tuple(this->key)));
          }
        };
        
        virtual std::size_t size() {
          if (this->useSubkey) {
            return this->_index.count(::boost::make_tuple(this->key,subkey));
          } else {
            return this->_index.count(::boost::make_tuple(this->key));
          }
        };

        template<typename ostream_type>
        void view(ostream_type& os, const bool& useColor = false, const char* label = NULL){
          if(label != NULL) {
            os << "Viewing " << label << " sequence:" << std::endl;
          } 
          os << "[";
          for(iterator i = this->begin(); i != this->end(); i++) {
            os << " (" << *i;
            if(useColor) {
              os << "," << i.color();
            }
            os  << ")";
          }
          os << " ]" << std::endl;
        };
      };// class ArrowSequence    
    };// class ArrowContainerTraits
  

    // The specialized ArrowContainer types distinguish the cases of unique and multiple colors of arrows on 
    // for each (source,target) pair (i.e., a single arrow, or multiple arrows between each pair of points).
    typedef enum {multiColor, uniColor} ColorMultiplicity;

    template<typename Source_, typename Target_, typename Color_, ColorMultiplicity colorMultiplicity> 
    struct ArrowContainer {};
    
    template<typename Source_, typename Target_, typename Color_>
    struct ArrowContainer<Source_, Target_, Color_, multiColor> {
      // Define container's encapsulated types
      typedef ArrowContainerTraits<Source_, Target_, Color_>      traits;
      // need to def arrow_type locally, since BOOST_MULTI_INDEX_MEMBER barfs when first template parameter starts with 'typename'
      typedef typename traits::arrow_type                         arrow_type; 
      // Container set type
      typedef ::boost::multi_index::multi_index_container<
        typename traits::arrow_type,
        ::boost::multi_index::indexed_by<
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<typename traits::sourceTargetTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::source_type, source), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::target_type, target)
            >
          >,
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<typename traits::sourceColorTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::source_type, source), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::color_type,  color)
            >
          >,
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<typename traits::targetColorTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::target_type, target), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::color_type,  color)
            >
          >
        >,
        ALE_ALLOCATOR<typename traits::arrow_type>
      > set_type;      
     // multi-index set of multicolor arrows
      set_type set;
    }; // class ArrowContainer<multiColor>
    
    template<typename Source_, typename Target_, typename Color_>
    struct ArrowContainer<Source_, Target_, Color_, uniColor> {
      // Define container's encapsulated types
      typedef ArrowContainerTraits<Source_, Target_, Color_>                traits;
      // need to def arrow_type locally, since BOOST_MULTI_INDEX_MEMBER barfs when first template parameter starts with 'typename'
      typedef typename traits::arrow_type                                   arrow_type; 

      // multi-index set type -- arrow set
      typedef ::boost::multi_index::multi_index_container<
        typename traits::arrow_type,
        ::boost::multi_index::indexed_by<
          ::boost::multi_index::ordered_unique<
            ::boost::multi_index::tag<typename traits::sourceTargetTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::source_type, source), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::target_type, target)
            >
          >,
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<typename traits::sourceColorTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::source_type, source), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::color_type,  color)
            >
          >,
          ::boost::multi_index::ordered_non_unique<
            ::boost::multi_index::tag<typename traits::targetColorTag>,
            ::boost::multi_index::composite_key<
              typename traits::arrow_type, 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::target_type, target), 
              BOOST_MULTI_INDEX_MEMBER(arrow_type, typename traits::color_type,  color)
            >
          >
        >,
        ALE_ALLOCATOR<typename traits::arrow_type>
      > set_type;      
      // multi-index set of unicolor arrow records 
      set_type set;
    }; // class ArrowContainer<uniColor>



    //
    // ColorBiGraph (short for ColorBipartiteGraph) implements a sequential interface similar to that of Sieve,
    // except the source and target points may have different types and iterated operations (e.g., nCone, closure)
    // are not available.
    // 


    template<typename Source_, typename Target_, typename Color_, ColorMultiplicity colorMultiplicity>
    class ColorBiGraph { // class ColorBiGraph
    public:
      typedef struct {
        // Encapsulated container types
        typedef ArrowContainer<Source_, Target_, Color_, colorMultiplicity>      arrow_container_type;
        typedef RecContainer<Source_>                                            cap_container_type;
        typedef RecContainer<Target_>                                            base_container_type;
        // Types associated with records held in containers
        typedef typename arrow_container_type::traits::arrow_type                arrow_type;
        typedef typename arrow_container_type::traits::source_type               source_type;
        typedef typename arrow_container_type::traits::target_type               target_type;
        typedef typename arrow_container_type::traits::color_type                color_type;
        // Convenient tag names
        typedef typename arrow_container_type::traits::sourceColorTag            supportInd;
        typedef typename arrow_container_type::traits::targetColorTag            coneInd;
        typedef typename arrow_container_type::traits::sourceTargetTag           arrowInd;
        typedef typename base_container_type::traits::degreeTag                  baseInd;
        typedef typename cap_container_type::traits::degreeTag                   capInd;
        //
        // Return types
        //
        typedef typename
        arrow_container_type::traits::template ArrowSequence<typename ::boost::multi_index::index<typename arrow_container_type::set_type,arrowInd>::type, source_type, target_type, BOOST_MULTI_INDEX_MEMBER(arrow_type, color_type, color)> 
        arrowSequence;

        typedef typename 
        arrow_container_type::traits::template ArrowSequence<typename ::boost::multi_index::index<typename arrow_container_type::set_type,coneInd>::type, target_type, color_type, BOOST_MULTI_INDEX_MEMBER(arrow_type, source_type, source)> 
        coneSequence;

        typedef typename 
        arrow_container_type::traits::template ArrowSequence<typename ::boost::multi_index::index<typename arrow_container_type::set_type, supportInd>::type, source_type, color_type, BOOST_MULTI_INDEX_MEMBER(arrow_type, target_type, target)> 
        supportSequence;
     
        typedef typename base_container_type::traits::DegreeSequence baseSequence;
        typedef typename cap_container_type::traits::DegreeSequence  capSequence;
        typedef std::set<source_type> coneSet;
        typedef std::set<target_type> supportSet;

      } traits;
    public:
      // Debug level
      int debug;
    protected:
      typename traits::arrow_container_type _arrows;
      typename traits::base_container_type  _base;
      typename traits::cap_container_type   _cap;
    public:
      // 
      // Basic interface
      //
      ColorBiGraph(int debug = 0) : debug(debug) {};
      virtual ~ColorBiGraph(){};
      //
      // Query methods
      //
      Obj<typename traits::capSequence>   
      cap() {
        return typename traits::capSequence(::boost::multi_index::get<typename traits::capInd>(_cap.set));
      };
      Obj<typename traits::baseSequence>    
      base() {
        return typename traits::baseSequence(::boost::multi_index::get<typename traits::baseInd>(_base.set));
      };
      Obj<typename traits::arrowSequence> 
      arrows(const typename traits::source_type& s, const typename traits::target_type& t) {
        return typename traits::arrowSequence(::boost::multi_index::get<typename traits::arrowInd>(this->_arrows.set), s, t);
      };
      Obj<typename traits::coneSequence> 
      cone(const typename traits::target_type& p) {
        return typename traits::coneSequence(::boost::multi_index::get<typename traits::coneInd>(this->_arrows.set), p);
      };
      template<class InputSequence> 
      Obj<typename traits::coneSet> 
      cone(const Obj<InputSequence>& points) {
        return this->cone(points, typename traits::color_type(), false);
      };
      Obj<typename traits::coneSequence> 
      cone(const typename traits::target_type& p, const typename traits::color_type& color) {
        return typename traits::coneSequence(::boost::multi_index::get<typename traits::coneInd>(this->_arrows.set), p, color);
      };
      template<class InputSequence>
      Obj<typename traits::coneSet> 
      cone(const Obj<InputSequence>& points, const typename traits::color_type& color, bool useColor = true) {
        Obj<typename traits::coneSet> cone = typename traits::coneSet();
        for(typename InputSequence::iterator p_itor = points->begin(); p_itor != points->end(); ++p_itor) {
          Obj<typename traits::coneSequence> pCone;
          if (useColor) {
            pCone = this->cone(*p_itor, color);
          } else {
            pCone = this->cone(*p_itor);
          }
          cone->insert(pCone->begin(), pCone->end());
        }
        return cone;
      };
      Obj<typename traits::supportSequence> 
      support(const typename traits::source_type& p) {
        return typename traits::supportSequence(::boost::multi_index::get<typename traits::supportInd>(this->_arrows.set), p);
      };
      Obj<typename traits::supportSequence> 
      support(const typename traits::source_type& p, const typename traits::color_type& color) {
        return typename traits::supportSequence(::boost::multi_index::get<typename traits::supportInd>(this->_arrows.set), p, color);
      };
      template<class sourceInputSequence>
      Obj<typename traits::supportSet>      
      support(const Obj<sourceInputSequence>& sources);
      // unimplemented
      template<class sourceInputSequence>
      Obj<typename traits::supportSet>      
      support(const Obj<sourceInputSequence>& sources, const typename traits::color_type& color);
      // unimplemented
 
      template<typename ostream_type>
      void view(ostream_type& os, const char* label = NULL, bool rawData = false){
        if(label != NULL) {
          os << "Viewing BiGraph '" << label << "':" << std::endl;
        } 
        else {
          os << "Viewing a BiGraph:" << std::endl;
        }
        if(!rawData) {
          os << "cap --> base:" << std::endl;
          typename traits::capSequence cap = this->cap();
          for(typename traits::capSequence::iterator capi = cap.begin(); capi != cap.end(); capi++) {
            typename traits::supportSequence supp = this->support(*capi);
            for(typename traits::supportSequence::iterator suppi = supp.begin(); suppi != supp.end(); suppi++) {
              os << *capi << "--(" << suppi.color() << ")-->" << *suppi << std::endl;
            }
          }
          os << "base <-- cap:" << std::endl;
          typename traits::baseSequence base = this->base();
          for(typename traits::baseSequence::iterator basei = base.begin(); basei != base.end(); basei++) {
            typename traits::coneSequence cone = this->cone(*basei);
            for(typename traits::coneSequence::iterator conei = cone.begin(); conei != cone.end(); conei++) {
              os << *basei <<  "<--(" << conei.color() << ")--" << *conei << std::endl;
            }
          }
          os << "cap --> outdegrees:" << std::endl;
          for(typename traits::capSequence::iterator capi = cap.begin(); capi != cap.end(); capi++) {
            os << *capi <<  "-->" << capi.degree() << std::endl;
          }
          os << "base <-- indegrees:" << std::endl;
          for(typename traits::baseSequence::iterator basei = base.begin(); basei != base.end(); basei++) {
            os << *basei <<  "<--" << basei.degree() << std::endl;
          }
        }
        else {
          os << "'raw' arrow set:" << std::endl;
          for(typename traits::arrow_container_type::set_type::iterator ai = _arrows.set.begin(); ai != _arrows.set.end(); ai++)
          {
            typename traits::arrow_type arr = *ai;
            os << arr << std::endl;
          }
          os << "'raw' base set:" << std::endl;
          for(typename traits::base_container_type::set_type::iterator bi = _base.set.begin(); bi != _base.set.end(); bi++) 
          {
            typename traits::base_container_type::traits::rec_type bp = *bi;
            os << bp << std::endl;
          }
          os << "'raw' cap set:" << std::endl;
          for(typename traits::cap_container_type::set_type::iterator ci = _cap.set.begin(); ci != _cap.set.end(); ci++) 
          {
            typename traits::cap_container_type::traits::rec_type cp = *ci;
            os << cp << std::endl;
          }
        }
      };
    public:
      //
      // Lattice queries
      //
      template<class targetInputSequence> 
      Obj<typename traits::coneSequence> meet(const Obj<targetInputSequence>& targets);
      // unimplemented
      template<class targetInputSequence> 
      Obj<typename traits::coneSequence> meet(const Obj<targetInputSequence>& targets, const typename traits::color_type& color);
      // unimplemented
      template<class sourceInputSequence> 
      Obj<typename traits::coneSequence> join(const Obj<sourceInputSequence>& sources);
      // unimplemented
      template<class sourceInputSequence> 
      Obj<typename traits::coneSequence> join(const Obj<sourceInputSequence>& sources, const typename traits::color_type& color);
    public:
      //
      // Structural manipulation
      //
      void clear() {
        this->_arrows.set.clear(); this->_base.set.clear(); this->_cap.set.clear();
      };
      void addArrow(const typename traits::source_type& p, const typename traits::target_type& q) {
        this->addArrow(p, q, typename traits::color_type());
      };
      void addArrow(const typename traits::source_type& p, const typename traits::target_type& q, const typename traits::color_type& color) {
        this->addArrow(typename traits::arrow_type(p, q, color));
        //std::cout << "Added " << arrow_type(p, q, color);
      };
      void addArrow(const typename traits::arrow_type& a) {
        this->_arrows.set.insert(a); _base.adjustDegree(a.target,1); _cap.adjustDegree(a.source,1);
        //std::cout << "Added " << Arrow_(p, q, color);
      };
      void removeArrow(const typename traits::arrow_type& a) {
        _base.adjustDegree(a.target, -1); _base.adjustDegree(a.source,-1);
        this->_arrows.set.erase(a); 
      };
      void addCone(const typename traits::source_type& source, const typename traits::target_type& target){
        this->addArrow(source, target);
      };
      template<class sourceInputSequence> 
      void addCone(const Obj<sourceInputSequence>& sources, const typename traits::target_type& target) {
        this->addCone(sources, target, typename traits::color_type());
      };
      void addCone(const typename traits::source_type& source, const typename traits::target_type& target, const typename traits::color_type& color) {
        this->addArrow(source, target, color);
      };
      template<class sourceInputSequence> 
      void 
      addCone(const Obj<sourceInputSequence>& sources, const typename traits::target_type& target, const typename traits::color_type& color) {
        if (debug) {std::cout << "Adding a cone " << std::endl;}
        for(typename sourceInputSequence::iterator iter = sources->begin(); iter != sources->end(); ++iter) {
          if (debug) {std::cout << "Adding arrow from " << *iter << " to " << target << "(" << color << ")" << std::endl;}
          this->addArrow(*iter, target, color);
        }
      };
      void clearCone(const typename traits::target_type& t) {
        clearCone(t, typename traits::color_type(), false);
      };
      void clearCone(const typename traits::target_type& t, const typename traits::color_type&  color, bool useColor = true) {
        // Use the cone sequence types to clear the cone
        typename traits::coneSequence::traits::index_type& coneIndex = 
          ::boost::multi_index::get<typename traits::coneInd>(this->_arrows.set);
        typename traits::coneSequence::traits::index_type::iterator i, ii, j;
        if(debug) {
          std::cout << "clearCone: removing cone over " << t;
          if(useColor) {
            std::cout << " with color" << color << std::endl;
            typename traits::coneSequence cone = this->cone(t,color);
            std::cout << "[" << std::endl;
            for(typename traits::coneSequence::iterator ci = cone.begin(); ci != cone.end(); ci++) {
              std::cout << "  " << ci.arrow();
            }
            std::cout << "]" << std::endl;
          }
          else {
            std::cout << std::endl;
            typename traits::coneSequence cone = this->cone(t);
            std::cout << "[" << std::endl;
            for(typename traits::coneSequence::iterator ci = cone.begin(); ci != cone.end(); ci++) {
              std::cout << "  " << ci.arrow();
            }
            std::cout << "]" << std::endl;
          }
        }
        if (useColor) {
           i = coneIndex.lower_bound(::boost::make_tuple(t,color));
           ii = coneIndex.upper_bound(::boost::make_tuple(t,color));
        } else {
          i = coneIndex.lower_bound(::boost::make_tuple(t));
          ii = coneIndex.upper_bound(::boost::make_tuple(t));
        }
        for(j = i; j != ii; j++){          
          // Adjust the degrees before removing the arrow; use a different iterator, since we'll need i,ii to do the arrow erasing.
          if(debug) {
            std::cout << "clearCone: adjusting degrees for endpoints of arrow: " << *j << std::endl;
          }
          this->_cap.adjustDegree(j->source, -1);
          this->_base.adjustDegree(j->target, -1);
        }
        coneIndex.erase(i,ii);
      };// clearCone()

      void clearSupport(const typename traits::source_type& s) {
        clearSupport(s, typename traits::color_type(), false);
      };
      void clearSupport(const typename traits::source_type& s, const typename traits::color_type&  color, bool useColor = true) {
        // Use the cone sequence types to clear the cone
        typename 
          traits::supportSequence::traits::index_type& suppIndex = ::boost::multi_index::get<typename traits::supportInd>(this->_arrows.set);
        typename traits::supportSequence::traits::index_type::iterator i, ii, j;
        if (useColor) {
          i = suppIndex.lower_bound(::boost::make_tuple(s,color));
          ii = suppIndex.upper_bound(::boost::make_tuple(s,color));
        } else {
          i = suppIndex.lower_bound(::boost::make_tuple(s));
          ii = suppIndex.upper_bound(::boost::make_tuple(s));
        }
        for(j = i; j != ii; j++){
          // Adjust the degrees before removing the arrow
          this->_cap.adjustDegree(j->source, -1);
          this->_base.adjustDegree(j->target, -1);
        }
        suppIndex.erase(i,ii);
      };
      void setCone(const typename traits::source_type& source, const typename traits::target_type& target){
        this->clearCone(target, typename traits::color_type(), false); this->addCone(source, target);
      };
      template<class sourceInputSequence> 
      void setCone(const Obj<sourceInputSequence>& sources, const typename traits::target_type& target) {
        this->clearCone(target, typename traits::color_type(), false); this->addCone(sources, target, typename traits::color_type());
      };
      void setCone(const typename traits::source_type& source, const typename traits::target_type& target, const typename traits::color_type& color) {
        this->clearCone(target, color, true); this->addCone(source, target, color);
      };
      template<class sourceInputSequence> 
      void setCone(const Obj<sourceInputSequence>& sources, const typename traits::target_type& target, const typename traits::color_type& color){
        this->clearCone(target, color, true); this->addCone(sources, target, color);
      };
      template<class targetInputSequence> 
      void addSupport(const typename traits::source_type& source, const Obj<targetInputSequence >& targets);
      // Unimplemented
      template<class targetInputSequence> 
      void addSupport(const typename traits::source_type& source, const Obj<targetInputSequence>& targets, const typename traits::color_type& color);
        
      void add(const Obj<ColorBiGraph<typename traits::source_type, typename traits::target_type, const typename traits::color_type, colorMultiplicity> >& cbg);
      // Unimplemented

    }; // class ColorBiGraph

    //
    // Delta namespace contains classes and methods implementing  the delta operation on a pair of ColorBiGraphs or similar objects.
    //
//     namespace Delta { 


//       template <typename LeftBiGraph_, typename RightBiGraph_, typename DeltaBiGraph_>
//       class ProductConeFuser {
//       public:
//         //Encapsulated types
//         struct traits {
//           typedef LeftBiGraph_  left_type;
//           typedef RightBiGraph_ right_type;
//           typedef DeltaBiGraph_ delta_type;
//           typedef std::pair<typename left_type::traits::source_type,typename right_type::traits::source_type> source_type;
//           typedef typename left_type::traits::target_type                                                     target_type;
//           typedef std::pair<typename left_type::traits::color_type,typename right_type::traits::color_type>   color_type;
//         };        
//         void
//         fuseCones(const typename traits::left_type::traits::coneSequence&  lcone, 
//                   const typename traits::right_type::traits::coneSequence& rcone, 
//                   typename typename traits::delta_type& delta) {
//           // This Fuser traverses both left cone and right cone, forming an arrow from each pair of arrows -- 
//           // one from each of the cones --  and inserting it into the delta BiGraph.
//           for(typename left_type::traits::coneSequence::iterator lci = lcone.begin(); lci != lcone.end(); lci++) {
//             for(typename left_type::traits::coneSequence::iterator lci = lcone.begin(); lci != lcone.end(); lci++) {
//               delta.addArrow(this->fuseArrows(lci.arrow(), rci.arrow()));
//             }
//           }
//         }
//         typename traits::delta_type::arrow_type
//         fuseArrows(const typename traits::left_type::traits::arrow_type& larrow, 
//                    const typename traits::right_type::traits::arrow_type& rarrow) {
//           return typename traits::arrow_type(traits::source_type(*lci,*rci), lci.target(), 
//                                              typename traits::color_type(lci.color(),rci.color()));
//         }
//       }; // struct ProductConeFuser
      

//       template <typename LeftBiGraph_, typename RightBiGraph_>
//       class BaseOverlapSequence : public LeftBiGraph_::traits::baseContainer::traits::PointSequence {
//       public:
//         // Encapsulted types
//         typedef LeftBiGraph_  left_type;
//         typedef RightBiGraph_ right_type;
//         typedef typename left_type::traits::baseContainer::traits::PointSequence::traits traits;
//         //
//         // Basic interface
//         //
//         BaseOverlapSequence(const typename traits::left_type& l, const typename traits::right_type& r) : _left(l), _right(r){};

//         virtual typename traits::iterator begin() {
//         };
//         virtual typename traits::iterator end() {
//           //return typename traits::iterator();
//         };
//       protected:
//         const typename traits::left_type&  _left;
//         const typename traits::right_type& _right;

//       };// class BaseOverlapSequence

//       template <typename LeftBiGraph_, typename RightBiGraph_, typename DeltaBiGraph_>
//       class ConeDelta {
//         // ConeDelta::operator() in various forms
//         void 
//         operator()(const left_type& l, const right_type& r, delta_type& d, const fuser_type& f = fuser_type()) {
//           // Compute the overlap of left and right bases and then call a 'based' version of the operator
//           operator()(overlap(l,r), l,r,d,f);
//         };
//         void 
//         operator()(const BaseOverlapSequence& overlap,const left_type& l,const right_type& r, delta_type& d, 
//                    const fuser_type& f = fuser_type()) {
//           for(typename BaseOverlapSequence::iterator i = overlap.begin(); i != overlap.end(); i++) {
//             typename left_type::traits::coneSequence lcone = l.cone(*i);
//             typename right_type::traits::coneSequence rcone = r.cone(*i);
//           }
//         }
//         Obj<delta_type> 
//         operator()(const left_type& l, const right_type& r, const fuser_type& f = fuser_type()) {
//           Obj<delta_type> d = delta_type();
//           operator()(l,r,d,f);
//           return d;
//         };
//         Obj<delta_type> 
//         operator()(const BaseOverlapSequence& overlap, const left_type& l, const right_type& r, const fuser_type& f = fuser_type()) {
//           Obj<delta_type> d = delta_type();
//           operator()(overlap,l,r,d,f);
//           return d;
//         };
//       }; // class ConeDelta
            
//     }; // namespace Delta

    // A UniColorBiGraph aka BiGraph
    template <typename Source_, typename Target_, typename Color_>
    class BiGraph : public ColorBiGraph<Source_, Target_, Color_, uniColor> {
    public:
      typedef typename ColorBiGraph<Source_, Target_, Color_, uniColor>::traits       traits;
      //typedef ColorBiGraphTraits<ColorBiGraph<Source_, Target_, Color_, uniColor> >   traits;
      // Re-export some typedefs expected by CoSifter
      typedef typename traits::arrow_type                                             Arrow_;
      typedef typename traits::coneSequence                                           coneSequence;
      typedef typename traits::supportSequence                                        supportSequence;
      typedef typename traits::baseSequence                                           baseSequence;
      typedef typename traits::capSequence                                            capSequence;
      // Basic interface
      BiGraph(const int& debug = 0) : ColorBiGraph<Source_, Target_, Color_, uniColor>(debug) {};
      
      const typename traits::color_type&
      getColor(const typename traits::source_type& s, const typename traits::target_type& t, bool fail = true) {
        typename traits::arrowSequence arr = this->arrows(s,t);
        if(arr.begin() != arr.end()) {
          return arr.begin().color();
        }
        if (fail) {
          ostringstream o;
          o << "Arrow " << s << " --> " << t << " not present";
          throw ALE::Exception(o.str().c_str());
        } else {
          static typename traits::color_type c;
          return c;
        }
      };

      template<typename ColorChanger>
      void modifyColor(const typename traits::source_type& s, const typename traits::target_type& t, const ColorChanger& changeColor) {
        typename ::boost::multi_index::index<typename traits::arrow_container_type::set_type, typename traits::arrowInd>::type& index = 
          ::boost::multi_index::get<typename traits::arrowInd>(this->_arrows.set);
        typename ::boost::multi_index::index<typename traits::arrow_container_type::set_type, typename traits::arrowInd>::type::iterator i = 
          index.find(::boost::make_tuple(s,t));
        if (i != index.end()) {
          index.modify(i, changeColor);
        } else {
          typename traits::arrow_type a(s, t, typename traits::color_type());
          changeColor(a);
          this->addArrow(a);
        }
      };
      
      struct ColorSetter {
        ColorSetter(const typename traits::color_type& color) : _color(color) {}; 
        void operator()(typename traits::arrow_type& p) const { 
          p.color = _color;
        } 
      private:
        const typename traits::color_type& _color;
      };

      void setColor(const typename traits::source_type& s, const typename traits::target_type& t, const typename traits::color_type& color) {
        ColorSetter colorSetter(color);
        typename ::boost::multi_index::index<typename traits::arrow_container_type::set_type, typename traits::arrowInd>::type& index = 
          ::boost::multi_index::get<typename traits::arrowInd>(this->_arrows.set);
        typename ::boost::multi_index::index<typename traits::arrow_container_type::set_type, typename traits::arrowInd>::type::iterator i = 
          index.find(::boost::make_tuple(s,t));
        if (i != index.end()) {
          index.modify(i, colorSetter);
        } else {
          typename traits::arrow_type a(s, t, color);
          this->addArrow(a);
        }
      };

    };// class BiGraph

  } // namespace Two

} // namespace ALE

#endif
