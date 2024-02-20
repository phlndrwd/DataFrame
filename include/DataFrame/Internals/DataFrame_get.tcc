// Hossein Moein
// September 12, 2017
/*
Copyright (c) 2019-2026, Hossein Moein
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Hossein Moein and/or the DataFrame nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Hossein Moein BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>

#include <cmath>
#include <functional>
#include <random>
#include <unordered_set>

// ----------------------------------------------------------------------------

namespace hmdf
{

template<typename I, typename H>
std::pair<typename DataFrame<I, H>::size_type,
          typename DataFrame<I, H>::size_type>
DataFrame<I, H>::shape() const  {

    return (std::make_pair(indices_.size(), column_list_.size()));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
MemUsage DataFrame<I, H>::get_memory_usage(const char *col_name) const  {

    MemUsage    result;

    result.index_type_size = sizeof(IndexType);
    result.column_type_size = sizeof(T);
    get_mem_numbers_(get_index(),
                     result.index_used_memory, result.index_capacity_memory);
    get_mem_numbers_(get_column<T>(col_name),
                     result.column_used_memory,
                     result.column_capacity_memory);
    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
typename DataFrame<I, H>::size_type
DataFrame<I, H>::col_name_to_idx (const char *col_name) const  {

    for (const auto &citer : column_list_)
        if (citer.first == col_name)
            return (citer.second);

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::col_name_to_idx(): ERROR: Cannot find column '%s'",
              col_name);
    throw ColNotFound (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
const char *
DataFrame<I, H>::col_idx_to_name (size_type col_idx) const  {

    for (const auto &citer : column_list_)
        if (citer.second == col_idx)
            return (citer.first.c_str());

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::col_idx_to_name(): ERROR: "
#ifdef _MSC_VER
              "Cannot find column index %zu",
#else
              "Cannot find column index %lu",
#endif // _MSC_VER
              col_idx);
    throw ColNotFound (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
typename DataFrame<I, H>::template ColumnVecType<T> &
DataFrame<I, H>::get_column (const char *name, bool do_lock)  {

    auto    iter = column_tb_.find (name);

    if (iter == column_tb_.end())  {
        char buffer [512];

        snprintf (buffer, sizeof(buffer) - 1,
                  "DataFrame::get_column(): ERROR: Cannot find column '%s'",
                  name);
        throw ColNotFound (buffer);
    }

    const SpinGuard guard (do_lock ? lock_ : nullptr);
    DataVec         &hv = data_[iter->second];
    auto            &data_vec = hv.template get_vector<T>();

    return (data_vec);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
typename DataFrame<I, H>::template ColumnVecType<typename T::type> &
DataFrame<I, H>::get_column ()  {

    return (get_column<typename T::type>(T::name));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
typename DataFrame<I, H>::template ColumnVecType<T> &
DataFrame<I, H>::get_column(size_type index, bool do_lock)  {

    return (get_column<T>(column_list_[index].first.c_str(), do_lock));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
bool DataFrame<I, H>::has_column (const char *name) const  {

    auto    iter = column_tb_.find (name);

    return (iter != column_tb_.end());
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
bool DataFrame<I, H>::
has_column(size_type index) const { return (index < column_list_.size()); }

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
const typename DataFrame<I, H>::template ColumnVecType<T> &
DataFrame<I, H>::get_column (const char *name, bool do_lock) const  {

    return (const_cast<DataFrame *>(this)->get_column<T>(name, do_lock));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
const typename DataFrame<I, H>::template ColumnVecType<typename T::type> &
DataFrame<I, H>::get_column () const  {

    return (const_cast<DataFrame *>(this)->get_column<typename T::type>(
                T::name));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
const typename DataFrame<I, H>::template ColumnVecType<T> &
DataFrame<I, H>::get_column(size_type index, bool do_lock) const  {

    return (get_column<T>(column_list_[index].first.c_str(), do_lock));
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
const typename DataFrame<I, H>::IndexVecType &
DataFrame<I, H>::get_index() const  { return (indices_); }

// ----------------------------------------------------------------------------

template<typename I, typename H>
typename DataFrame<I, H>::IndexVecType &
DataFrame<I, H>::get_index()  { return (indices_); }

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
HeteroVector<std::size_t(H::align_value)> DataFrame<I, H>::
get_row(size_type row_num, const StlVecType<const char *> &col_names) const {

    if (row_num >= indices_.size())  {
        char buffer [512];

#ifdef _MSC_VER
        snprintf(buffer, sizeof(buffer) - 1,
                 "DataFrame::get_row(): ERROR: There aren't %zu rows",
#else
        snprintf(buffer, sizeof(buffer) - 1,
                 "DataFrame::get_row(): ERROR: There aren't %lu rows",
#endif // _MSC_VER
                 row_num);
        throw BadRange(buffer);
    }

    HeteroVector<align_value>   ret_vec;

    ret_vec.template reserve<IndexType>(1);
    ret_vec.push_back(indices_[row_num]);

    get_row_functor_<Ts ...>    functor(ret_vec, row_num);
    const SpinGuard             guard(lock_);

    for (const auto &name_citer : col_names)  {
        const auto  &citer = column_tb_.find (name_citer);

        if (citer == column_tb_.end())  {
            char buffer [512];

            snprintf(buffer, sizeof(buffer) - 1,
                     "DataFrame::get_row(): ERROR: Cannot find column '%s'",
                     name_citer);
            throw ColNotFound(buffer);
        }

        data_[citer->second].change(functor);
    }

    return (ret_vec);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
HeteroVector<std::size_t(H::align_value)> DataFrame<I, H>::
get_row(size_type row_num) const {

    if (row_num >= indices_.size())  {
        char buffer [512];

#ifdef _MSC_VER
        snprintf(buffer, sizeof(buffer) - 1,
                 "DataFrame::get_row(): ERROR: There aren't %zu rows",
#else
        snprintf(buffer, sizeof(buffer) - 1,
                 "DataFrame::get_row(): ERROR: There aren't %lu rows",
#endif // _MSC_VER
                 row_num);
        throw BadRange(buffer);
    }

    HeteroVector<align_value>   ret_vec;

    ret_vec.template reserve<IndexType>(1);
    ret_vec.push_back(indices_[row_num]);

    get_row_functor_<Ts ...>    functor(ret_vec, row_num);
    const SpinGuard             guard(lock_);

    for (const auto &citer : column_list_)
        data_[citer.second].change(functor);

    return (ret_vec);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
typename DataFrame<I, H>::template ColumnVecType<T> DataFrame<I, H>::
get_col_unique_values(const char *name) const  {

    const ColumnVecType<T>  &vec = get_column<T>(name);
    auto                    hash_func =
        [](std::reference_wrapper<const T> v) -> std::size_t  {
            return(std::hash<T>{}(v.get()));
    };
    auto                    equal_func =
        [](std::reference_wrapper<const T> lhs,
           std::reference_wrapper<const T> rhs) -> bool  {
            return(lhs.get() == rhs.get());
    };

    std::unordered_set<
        typename std::reference_wrapper<T>::type,
        decltype(hash_func),
        decltype(equal_func)>   table(vec.size(), hash_func, equal_func);
    bool                        counted_nan = false;
    ColumnVecType<T>            result;

    result.reserve(vec.size());
    for (const auto &citer : vec)  {
        if (is_nan<T>(citer) && ! counted_nan)  {
            counted_nan = true;
            result.push_back(get_nan<T>());
            continue;
        }

        const auto  insert_ret = table.emplace(std::ref(citer));

        if (insert_ret.second)
            result.push_back(citer);
    }

    return(result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H>
DataFrame<I, H>::get_data_by_idx (Index2D<IndexType> range) const  {

    const auto  &lower =
        std::lower_bound (indices_.begin(), indices_.end(), range.begin);
    const auto  &upper =
        std::upper_bound (indices_.begin(), indices_.end(), range.end);
    DataFrame   df;

    if (lower != indices_.end())  {
        df.load_index(lower, upper);

        const size_type b_dist = std::distance(indices_.begin(), lower);
        const size_type e_dist = std::distance(indices_.begin(),
                                               upper < indices_.end()
                                                   ? upper
                                                   : indices_.end());

        const SpinGuard guard(lock_);

        for (const auto &citer : column_list_)  {
            load_functor_<DataFrame, Ts ...>    functor (citer.first.c_str(),
                                                         b_dist,
                                                         e_dist,
                                                         df);

            data_[citer.second].change(functor);
        }
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_idx(const StlVecType<IndexType> &values) const  {

    const std::unordered_set<IndexType> val_table(values.begin(), values.end());
    IndexVecType                        new_index;
    StlVecType<size_type>              locations;
    const size_type                     values_s = values.size();
    const size_type                     idx_s = indices_.size();

    new_index.reserve(values_s);
    locations.reserve(values_s);
    for (size_type i = 0; i < idx_s; ++i)
        if (val_table.find(indices_[i]) != val_table.end())  {
            new_index.push_back(indices_[i]);
            locations.push_back(i);
        }

    DataFrame   df;

    df.load_index(std::move(new_index));

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            locations,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::View
DataFrame<I, H>::get_view_by_idx (Index2D<IndexType> range)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_idx()");

    auto    lower =
        std::lower_bound (indices_.begin(), indices_.end(), range.begin);
    auto    upper =
        std::upper_bound (indices_.begin(), indices_.end(), range.end);
    View    dfv;

    if (lower != indices_.end() &&
        (upper != indices_.end() || indices_.back() == range.end))  {
        IndexType       *upper_address = nullptr;
        const size_type b_dist = std::distance(indices_.begin(), lower);
        const size_type e_dist = std::distance(indices_.begin(), upper);

        if (upper != indices_.end())
            upper_address = &*upper;
        else
            upper_address = &(indices_.front()) + e_dist;
        dfv.indices_ = typename View::IndexVecType(&*lower, upper_address);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            view_setup_functor_<View, Ts ...>   functor (iter.first.c_str(),
                                                         b_dist,
                                                         e_dist,
                                                         dfv);

            data_[iter.second].change(functor);
        }
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstView
DataFrame<I, H>::get_view_by_idx (Index2D<IndexType> range) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_idx()");

    auto        lower =
        std::lower_bound (indices_.begin(), indices_.end(), range.begin);
    auto        upper =
        std::upper_bound (indices_.begin(), indices_.end(), range.end);
    ConstView   dfcv;

    if (lower != indices_.end() &&
        (upper != indices_.end() || indices_.back() == range.end))  {
        const IndexType *upper_address = nullptr;
        const size_type b_dist = std::distance(indices_.begin(), lower);
        const size_type e_dist = std::distance(indices_.begin(), upper);

        if (upper != indices_.end())
            upper_address = &*upper;
        else
            upper_address = &(indices_.front()) + e_dist;
        dfcv.indices_ =
            typename ConstView::IndexVecType(&*lower, upper_address);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            view_setup_functor_<ConstView, Ts ...>  functor (iter.first.c_str(),
                                                             b_dist,
                                                             e_dist,
                                                             dfcv);

            data_[iter.second].change(functor);
        }
    }

    return (dfcv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_idx(const StlVecType<IndexType> &values)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_idx()");

    using TheView = PtrView;

    const std::unordered_set<IndexType> val_table(values.begin(),
                                                  values.end());
    typename TheView::IndexVecType  new_index;
    StlVecType<size_type>           locations;
    const size_type                 values_s = values.size();
    const size_type                 idx_s = indices_.size();

    new_index.reserve(values_s);
    locations.reserve(values_s);

    for (size_type i = 0; i < idx_s; ++i)
        if (val_table.find(indices_[i]) != val_table.end())  {
            new_index.push_back(&(indices_[i]));
            locations.push_back(i);
        }

    TheView dfv;

    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            locations,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_idx(const StlVecType<IndexType> &values) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_idx()");

    using TheView = ConstPtrView;

    const std::unordered_set<IndexType> val_table(values.begin(),
                                                  values.end());
    typename TheView::IndexVecType  new_index;
    StlVecType<size_type>           locations;
    const size_type                 values_s = values.size();
    const size_type                 idx_s = indices_.size();

    new_index.reserve(values_s);
    locations.reserve(values_s);

    for (size_type i = 0; i < idx_s; ++i)
        if (val_table.find(indices_[i]) != val_table.end())  {
            new_index.push_back(&(indices_[i]));
            locations.push_back(i);
        }

    TheView dfv;

    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>
            functor (col_citer.first.c_str(),
                     locations,
                     idx_s,
                     dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H>
DataFrame<I, H>::get_data_by_loc (Index2D<long> range) const  {

    if (range.begin < 0)
        range.begin = static_cast<long>(indices_.size()) + range.begin;
    if (range.end < 0)
        range.end = static_cast<long>(indices_.size()) + range.end + 1;

    if (range.end <= static_cast<long>(indices_.size()) &&
        range.begin <= range.end && range.begin >= 0)  {
        DataFrame   df;

        df.load_index(indices_.begin() + static_cast<size_type>(range.begin),
                      indices_.begin() + static_cast<size_type>(range.end));

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            load_functor_<DataFrame, Ts ...>    functor (
                iter.first.c_str(),
                static_cast<size_type>(range.begin),
                static_cast<size_type>(range.end),
                df);

            data_[iter.second].change(functor);
        }

        return (df);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_data_by_loc(): ERROR: "
              "Bad begin, end range: %ld, %ld",
              range.begin, range.end);
    throw BadRange (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_loc (const StlVecType<long> &locations) const  {

    const size_type idx_s = indices_.size();
    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(locations.size());
    for (auto citer : locations)  {
        const size_type index =
            citer >= 0 ? (citer) : (citer) + static_cast<long>(idx_s);

        new_index.push_back(indices_[index]);
    }
    df.load_index(std::move(new_index));

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<long, Ts ...>  functor (
            col_citer.first.c_str(),
            locations,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::View
DataFrame<I, H>::get_view_by_loc (Index2D<long> range)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_loc()");

    const long  idx_s = static_cast<long>(indices_.size());

    if (range.begin < 0)
        range.begin = idx_s + range.begin;
    if (range.end < 0)
        range.end = idx_s + range.end + 1;

    if (range.end <= idx_s && range.begin <= range.end && range.begin >= 0)  {
        View    dfv;

        dfv.indices_ =
            typename View::IndexVecType(&(indices_[0]) + range.begin,
                                        &(indices_[0]) + range.end);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            view_setup_functor_<View, Ts ...>   functor (
                iter.first.c_str(),
                static_cast<size_type>(range.begin),
                static_cast<size_type>(range.end),
                dfv);

            data_[iter.second].change(functor);
        }

        return (dfv);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_view_by_loc(): ERROR: "
              "Bad begin, end range: %ld, %ld",
              range.begin, range.end);
    throw BadRange (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstView
DataFrame<I, H>::get_view_by_loc (Index2D<long> range) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_loc()");

    const long  idx_s = static_cast<long>(indices_.size());

    if (range.begin < 0)
        range.begin = idx_s + range.begin;
    if (range.end < 0)
        range.end = idx_s + range.end + 1;

    if (range.end <= idx_s && range.begin <= range.end && range.begin >= 0)  {
        ConstView   dfcv;

        dfcv.indices_ =
            typename ConstView::IndexVecType(&(indices_[0]) + range.begin,
                                             &(indices_[0]) + range.end);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            view_setup_functor_<ConstView, Ts ...>  functor(
                iter.first.c_str(),
                static_cast<size_type>(range.begin),
                static_cast<size_type>(range.end),
                dfcv);

            data_[iter.second].change(functor);
        }

        return (dfcv);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_view_by_loc(): ERROR: "
              "Bad begin, end range: %ld, %ld",
              range.begin, range.end);
    throw BadRange(buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::PtrView
DataFrame<I, H>::get_view_by_loc (const StlVecType<long> &locations)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_loc()");

    using TheView = PtrView;

    TheView         dfv;
    const size_type idx_s = indices_.size();

    typename TheView::IndexVecType  new_index;

    new_index.reserve(locations.size());
    for (auto citer: locations)  {
        const size_type index =
            citer >= 0 ? (citer) : (citer) + static_cast<long>(idx_s);

        new_index.push_back(&(indices_[index]));
    }
    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<long, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            locations,
            indices_.size(),
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_loc (const StlVecType<long> &locations) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_loc()");

    using TheView = ConstPtrView;

    TheView         dfv;
    const size_type idx_s = indices_.size();

    typename TheView::IndexVecType  new_index;

    new_index.reserve(locations.size());
    for (auto citer: locations)  {
        const size_type index =
            citer >= 0 ? (citer) : (citer) + static_cast<long>(idx_s);

        new_index.push_back(&(indices_[index]));
    }
    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<long, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            locations,
            indices_.size(),
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel (const char *name, F &sel_functor) const  {

    const ColumnVecType<T>  &vec = get_column<T>(name);
    const size_type         idx_s = indices_.size();
    const size_type         col_s = vec.size();
    StlVecType<size_type>  col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < col_s; ++i)
        if (sel_functor (indices_[i], vec[i]))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename F, typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_sel (const char *name, F &sel_functor)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const ColumnVecType<T>  &vec = get_column<T>(name);
    const size_type         idx_s = indices_.size();
    const size_type         col_s = vec.size();
    StlVecType<size_type>  col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < col_s; ++i)
        if (sel_functor (indices_[i], vec[i]))
            col_indices.push_back(i);

    using TheView = PtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename F, typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_sel (const char *name, F &sel_functor) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const ColumnVecType<T>  &vec = get_column<T>(name);
    const size_type         idx_s = indices_.size();
    const size_type         col_s = vec.size();
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < col_s; ++i)
        if (sel_functor (indices_[i], vec[i]))
            col_indices.push_back(i);

    using TheView = ConstPtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel (const char *name1, const char *name2, F &sel_functor) const  {

    const size_type         idx_s = indices_.size();
    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         min_col_s = std::min(col_s1, col_s2);
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename F, typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_sel (const char *name1, const char *name2, F &sel_functor)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         min_col_s = std::min(col_s1, col_s2);
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>()))
            col_indices.push_back(i);

    using TheView = PtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename F, typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_sel (const char *name1, const char *name2, F &sel_functor) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         min_col_s = std::min(col_s1, col_s2);
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>()))
            col_indices.push_back(i);

    using TheView = ConstPtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel (const char *name1,
                 const char *name2,
                 const char *name3,
                 F &sel_functor) const  {

    const size_type         idx_s = indices_.size();
    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         min_col_s = std::min({ col_s1, col_s2, col_s3 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>(),
                         i < col_s3 ? vec3[i] : get_nan<T3>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename Tuple, typename F, typename... FilterCols>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel (F &sel_functor) const  {

    const size_type idx_s = indices_.size();
    // Get columns to std::tuple
    std::tuple      cols_for_filter(
        get_column<typename FilterCols::type>(FilterCols::name)...);
    // Calculate the max size in all columns
    size_type       col_s = 0;

    std::apply([&](auto && ... col)  {
                   using expander = int[];

                   (void) expander { 0, ((col_s = col.size() > col_s ?
                                         col.size() : col_s), 0) ... };
               },
               cols_for_filter);

    // Get the index of all records that meet the filters
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < col_s; ++i)  {
        std::apply(
            [&](auto && ... col)  {
                if (sel_functor(indices_[i],
                                (i < col.size() ?
                                 col[i] :
                                 // Get default value based on vec::value_type
                                 get_nan<typename std::decay<decltype(col)>
                                 ::type::value_type>())...))  {
                    col_indices.push_back(i);
                }
            },
            cols_for_filter);
    }

    // Get the records based on indices
    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Tuple>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename Tuple, typename F, typename... FilterCols>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel (F &sel_functor, FilterCols && ... filter_cols) const  {

    const size_type idx_s = indices_.size();
    // Get columns to std::tuple
    std::tuple      cols_for_filter(
        get_column<typename std::decay<decltype(filter_cols)>::type::type>(
            filter_cols.col_name())...);
    // Calculate the max size in all columns
    size_type       col_s = 0;

    std::apply([&](auto&&... col) {
                   using expander = int[];
                   (void) expander {0, ((col_s = col.size() > col_s ?
                                         col.size() : col_s), 0) ... };
               },
               cols_for_filter);

    // Get the index of all records that meet the filters
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < col_s; ++i)  {
        std::apply(
            [&](auto&&... col) {
                if (sel_functor(indices_[i],
                                (i < col.size() ?
                                 col[i] :
                                 get_nan<typename std::decay<decltype(col)>
                                 ::type::value_type>())...))
                    col_indices.push_back(i);
            },
            cols_for_filter);
    }

    DataFrame       df;
    IndexVecType    new_index;

    // Get the records based on indices
    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    const SpinGuard guard(lock_);

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Tuple>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename F, typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_sel (const char *name1,
                 const char *name2,
                 const char *name3,
                 F &sel_functor)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         min_col_s = std::min({ col_s1, col_s2, col_s3 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>(),
                         i < col_s3 ? vec3[i] : get_nan<T3>()))
            col_indices.push_back(i);

    using TheView = PtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename F, typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_sel (const char *name1,
                 const char *name2,
                 const char *name3,
                 F &sel_functor) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         min_col_s = std::min({ col_s1, col_s2, col_s3 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor (indices_[i],
                         i < col_s1 ? vec1[i] : get_nan<T1>(),
                         i < col_s2 ? vec2[i] : get_nan<T2>(),
                         i < col_s3 ? vec3[i] : get_nan<T3>()))
            col_indices.push_back(i);

    using TheView = ConstPtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4,
         typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                F &sel_functor) const  {

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i], vec4[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4,
         typename F, typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                F &sel_functor)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i], vec4[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>()))
            col_indices.push_back(i);

    using TheView = PtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4,
         typename F, typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                F &sel_functor) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i], vec1[i], vec2[i], vec3[i], vec4[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>()))
            col_indices.push_back(i);

    using TheView = ConstPtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                F &sel_functor) const  {

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5> &vec5 = get_column<T5>(name5, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         col_s5 = vec5.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8, typename T9, typename T10,
         typename T11,
         typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                const char *name6,
                const char *name7,
                const char *name8,
                const char *name9,
                const char *name10,
                const char *name11,
                F &sel_functor) const  {

    const SpinGuard          guard (lock_);
    const ColumnVecType<T1>  &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2>  &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3>  &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4>  &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5>  &vec5 = get_column<T5>(name5, false);
    const ColumnVecType<T6>  &vec6 = get_column<T6>(name6, false);
    const ColumnVecType<T7>  &vec7 = get_column<T7>(name7, false);
    const ColumnVecType<T8>  &vec8 = get_column<T8>(name8, false);
    const ColumnVecType<T9>  &vec9 = get_column<T9>(name9, false);
    const ColumnVecType<T10> &vec10 = get_column<T10>(name10, false);
    const ColumnVecType<T11> &vec11 = get_column<T11>(name11, false);
    const size_type          idx_s = indices_.size();
    const size_type          col_s1 = vec1.size();
    const size_type          col_s2 = vec2.size();
    const size_type          col_s3 = vec3.size();
    const size_type          col_s4 = vec4.size();
    const size_type          col_s5 = vec5.size();
    const size_type          col_s6 = vec6.size();
    const size_type          col_s7 = vec7.size();
    const size_type          col_s8 = vec8.size();
    const size_type          col_s9 = vec9.size();
    const size_type          col_s10 = vec10.size();
    const size_type          col_s11 = vec11.size();
    const size_type          min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5,
                   col_s6, col_s7, col_s8, col_s9, col_s10,
                   col_s11 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i],
                        vec6[i], vec7[i], vec8[i], vec9[i], vec10[i],
                        vec11[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>(),
                        i < col_s6 ? vec6[i] : get_nan<T6>(),
                        i < col_s7 ? vec7[i] : get_nan<T7>(),
                        i < col_s8 ? vec8[i] : get_nan<T8>(),
                        i < col_s9 ? vec9[i] : get_nan<T9>(),
                        i < col_s10 ? vec10[i] : get_nan<T10>(),
                        i < col_s11 ? vec11[i] : get_nan<T11>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8, typename T9, typename T10,
         typename T11, typename T12,
         typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                const char *name6,
                const char *name7,
                const char *name8,
                const char *name9,
                const char *name10,
                const char *name11,
                const char *name12,
                F &sel_functor) const  {

    const SpinGuard          guard (lock_);
    const ColumnVecType<T1>  &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2>  &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3>  &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4>  &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5>  &vec5 = get_column<T5>(name5, false);
    const ColumnVecType<T6>  &vec6 = get_column<T6>(name6, false);
    const ColumnVecType<T7>  &vec7 = get_column<T7>(name7, false);
    const ColumnVecType<T8>  &vec8 = get_column<T8>(name8, false);
    const ColumnVecType<T9>  &vec9 = get_column<T9>(name9, false);
    const ColumnVecType<T10> &vec10 = get_column<T10>(name10, false);
    const ColumnVecType<T11> &vec11 = get_column<T11>(name11, false);
    const ColumnVecType<T12> &vec12 = get_column<T12>(name12, false);
    const size_type          idx_s = indices_.size();
    const size_type          col_s1 = vec1.size();
    const size_type          col_s2 = vec2.size();
    const size_type          col_s3 = vec3.size();
    const size_type          col_s4 = vec4.size();
    const size_type          col_s5 = vec5.size();
    const size_type          col_s6 = vec6.size();
    const size_type          col_s7 = vec7.size();
    const size_type          col_s8 = vec8.size();
    const size_type          col_s9 = vec9.size();
    const size_type          col_s10 = vec10.size();
    const size_type          col_s11 = vec11.size();
    const size_type          col_s12 = vec12.size();
    const size_type          min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5,
                   col_s6, col_s7, col_s8, col_s9, col_s10,
                   col_s11, col_s12 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i],
                        vec6[i], vec7[i], vec8[i], vec9[i], vec10[i],
                        vec11[i], vec12[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>(),
                        i < col_s6 ? vec6[i] : get_nan<T6>(),
                        i < col_s7 ? vec7[i] : get_nan<T7>(),
                        i < col_s8 ? vec8[i] : get_nan<T8>(),
                        i < col_s9 ? vec9[i] : get_nan<T9>(),
                        i < col_s10 ? vec10[i] : get_nan<T10>(),
                        i < col_s11 ? vec11[i] : get_nan<T11>(),
                        i < col_s12 ? vec12[i] : get_nan<T12>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8, typename T9, typename T10,
         typename T11, typename T12, typename T13,
         typename F, typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                const char *name6,
                const char *name7,
                const char *name8,
                const char *name9,
                const char *name10,
                const char *name11,
                const char *name12,
                const char *name13,
                F &sel_functor) const  {

    const SpinGuard          guard (lock_);
    const ColumnVecType<T1>  &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2>  &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3>  &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4>  &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5>  &vec5 = get_column<T5>(name5, false);
    const ColumnVecType<T6>  &vec6 = get_column<T6>(name6, false);
    const ColumnVecType<T7>  &vec7 = get_column<T7>(name7, false);
    const ColumnVecType<T8>  &vec8 = get_column<T8>(name8, false);
    const ColumnVecType<T9>  &vec9 = get_column<T9>(name9, false);
    const ColumnVecType<T10> &vec10 = get_column<T10>(name10, false);
    const ColumnVecType<T11> &vec11 = get_column<T11>(name11, false);
    const ColumnVecType<T12> &vec12 = get_column<T12>(name12, false);
    const ColumnVecType<T13> &vec13 = get_column<T13>(name13, false);
    const size_type          idx_s = indices_.size();
    const size_type          col_s1 = vec1.size();
    const size_type          col_s2 = vec2.size();
    const size_type          col_s3 = vec3.size();
    const size_type          col_s4 = vec4.size();
    const size_type          col_s5 = vec5.size();
    const size_type          col_s6 = vec6.size();
    const size_type          col_s7 = vec7.size();
    const size_type          col_s8 = vec8.size();
    const size_type          col_s9 = vec9.size();
    const size_type          col_s10 = vec10.size();
    const size_type          col_s11 = vec11.size();
    const size_type          col_s12 = vec12.size();
    const size_type          col_s13 = vec13.size();
    const size_type          min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5,
                   col_s6, col_s7, col_s8, col_s9, col_s10,
                   col_s11, col_s12, col_s13 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i],
                        vec6[i], vec7[i], vec8[i], vec9[i], vec10[i],
                        vec11[i], vec12[i], vec13[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>(),
                        i < col_s6 ? vec6[i] : get_nan<T6>(),
                        i < col_s7 ? vec7[i] : get_nan<T7>(),
                        i < col_s8 ? vec8[i] : get_nan<T8>(),
                        i < col_s9 ? vec9[i] : get_nan<T9>(),
                        i < col_s10 ? vec10[i] : get_nan<T10>(),
                        i < col_s11 ? vec11[i] : get_nan<T11>(),
                        i < col_s12 ? vec12[i] : get_nan<T12>(),
                        i < col_s13 ? vec13[i] : get_nan<T13>()))
            col_indices.push_back(i);

    DataFrame       df;
    IndexVecType    new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(indices_[citer]);
    df.load_index(std::move(new_index));

    for (const auto &col_citer : column_list_)  {
        sel_load_functor_<size_type, Ts ...>    functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            df);

        data_[col_citer.second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename F, typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                F &sel_functor)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5> &vec5 = get_column<T5>(name5, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         col_s5 = vec5.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>()))
            col_indices.push_back(i);

    using TheView = PtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T1, typename T2, typename T3, typename T4, typename T5,
         typename F, typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_sel(const char *name1,
                const char *name2,
                const char *name3,
                const char *name4,
                const char *name5,
                F &sel_functor) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view_by_sel()");

    const SpinGuard         guard (lock_);
    const ColumnVecType<T1> &vec1 = get_column<T1>(name1, false);
    const ColumnVecType<T2> &vec2 = get_column<T2>(name2, false);
    const ColumnVecType<T3> &vec3 = get_column<T3>(name3, false);
    const ColumnVecType<T4> &vec4 = get_column<T4>(name4, false);
    const ColumnVecType<T5> &vec5 = get_column<T5>(name5, false);
    const size_type         idx_s = indices_.size();
    const size_type         col_s1 = vec1.size();
    const size_type         col_s2 = vec2.size();
    const size_type         col_s3 = vec3.size();
    const size_type         col_s4 = vec4.size();
    const size_type         col_s5 = vec5.size();
    const size_type         min_col_s =
        std::min({ col_s1, col_s2, col_s3, col_s4, col_s5 });
    StlVecType<size_type>   col_indices;

    col_indices.reserve(idx_s / 2);
    for (size_type i = 0; i < min_col_s; ++i)
        if (sel_functor(indices_[i],
                        vec1[i], vec2[i], vec3[i], vec4[i], vec5[i]))
            col_indices.push_back(i);
    for (size_type i = min_col_s; i < idx_s; ++i)
        if (sel_functor(indices_[i],
                        i < col_s1 ? vec1[i] : get_nan<T1>(),
                        i < col_s2 ? vec2[i] : get_nan<T2>(),
                        i < col_s3 ? vec3[i] : get_nan<T3>(),
                        i < col_s4 ? vec4[i] : get_nan<T4>(),
                        i < col_s5 ? vec5[i] : get_nan<T5>()))
            col_indices.push_back(i);

    using TheView = ConstPtrView;

    TheView                         dfv;
    typename TheView::IndexVecType  new_index;

    new_index.reserve(col_indices.size());
    for (auto citer: col_indices)
        new_index.push_back(&(indices_[citer]));
    dfv.indices_ = std::move(new_index);

    for (const auto &col_citer : column_list_)  {
        sel_load_view_functor_<size_type, TheView, Ts ...>   functor (
            col_citer.first.c_str(),
            col_indices,
            idx_s,
            dfv);

        data_[col_citer.second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data_by_rand(random_policy spec, double n, size_type seed) const  {

    bool            use_seed = false;
    size_type       n_rows = static_cast<size_type>(n);
    const size_type index_s = indices_.size();

    if (spec == random_policy::num_rows_with_seed)  {
        use_seed = true;
    }
    else if (spec == random_policy::frac_rows_with_seed)  {
        use_seed = true;
        n_rows = static_cast<size_type>(n * double(index_s));
    }
    else if (spec == random_policy::frac_rows_no_seed)  {
        n_rows = static_cast<size_type>(n * double(index_s));
    }

    if (index_s > 0 && n_rows <= index_s)  {
        std::random_device  rd;
        std::mt19937        gen(rd());

        if (use_seed)  gen.seed(static_cast<unsigned int>(seed));

        std::uniform_int_distribution<size_type>    dis(0, index_s - 1);
        StlVecType<size_type>                       rand_indices(n_rows);

        for (size_type i = 0; i < n_rows; ++i)
            rand_indices[i] = dis(gen);
        std::sort(rand_indices.begin(), rand_indices.end());

        IndexVecType    new_index;
        size_type       prev_value;

        new_index.reserve(n_rows);
        for (size_type i = 0; i < n_rows; ++i)  {
            if (i == 0 || rand_indices[i] != prev_value)
                new_index.push_back(indices_[rand_indices[i]]);
            prev_value = rand_indices[i];
        }

        DataFrame   df;

        df.load_index(std::move(new_index));

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            random_load_data_functor_<Ts ...>   functor (
                iter.first.c_str(),
                rand_indices,
                df);

            data_[iter.second].change(functor);
        }

        return (df);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_data_by_rand(): ERROR: "
#ifdef _MSC_VER
              "Number of rows requested %zu is more than available rows %zu",
#else
              "Number of rows requested %lu is more than available rows %lu",
#endif // _MSC_VER
              n_rows, index_s);
    throw BadRange (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::PtrView DataFrame<I, H>::
get_view_by_rand (random_policy spec, double n, size_type seed)  {

    bool            use_seed = false;
    size_type       n_rows = static_cast<size_type>(n);
    const size_type index_s = indices_.size();

    if (spec == random_policy::num_rows_with_seed)  {
        use_seed = true;
    }
    else if (spec == random_policy::frac_rows_with_seed)  {
        use_seed = true;
        n_rows = static_cast<size_type>(n * double(index_s));
    }
    else if (spec == random_policy::frac_rows_no_seed)  {
        n_rows = static_cast<size_type>(n * double(index_s));
    }

    if (index_s > 0 && n_rows <= index_s)  {
        std::random_device  rd;
        std::mt19937        gen(rd());

        if (use_seed)  gen.seed(static_cast<unsigned int>(seed));

        std::uniform_int_distribution<size_type>    dis(0, index_s - 1);
        StlVecType<size_type>                       rand_indices(n_rows);

        for (size_type i = 0; i < n_rows; ++i)
            rand_indices[i] = dis(gen);
        std::sort(rand_indices.begin(), rand_indices.end());

        using TheView = PtrView;

        typename TheView::IndexVecType  new_index;
        size_type                       prev_value;

        new_index.reserve(n_rows);
        for (size_type i = 0; i < n_rows; ++i)  {
            if (i == 0 || rand_indices[i] != prev_value)
                new_index.push_back(&(indices_[rand_indices[i]]));
            prev_value = rand_indices[i];
        }

        TheView dfv;

        dfv.indices_ = std::move(new_index);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            random_load_view_functor_<TheView, Ts ...>
                functor (iter.first.c_str(), rand_indices, dfv);

            data_[iter.second].change(functor);
        }

        return (dfv);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_view_by_rand(): ERROR: "
#ifdef _MSC_VER
              "Number of rows requested %zu is more than available rows %zu",
#else
              "Number of rows requested %lu is more than available rows %lu",
#endif // _MSC_VER
              n_rows, index_s);
    throw BadRange (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstPtrView
DataFrame<I, H>::
get_view_by_rand (random_policy spec, double n, size_type seed) const  {

    bool            use_seed = false;
    size_type       n_rows = static_cast<size_type>(n);
    const size_type index_s = indices_.size();

    if (spec == random_policy::num_rows_with_seed)  {
        use_seed = true;
    }
    else if (spec == random_policy::frac_rows_with_seed)  {
        use_seed = true;
        n_rows = static_cast<size_type>(n * double(index_s));
    }
    else if (spec == random_policy::frac_rows_no_seed)  {
        n_rows = static_cast<size_type>(n * double(index_s));
    }

    if (index_s > 0 && n_rows <= index_s)  {
        std::random_device  rd;
        std::mt19937        gen(rd());

        if (use_seed)  gen.seed(static_cast<unsigned int>(seed));

        std::uniform_int_distribution<size_type>    dis(0, index_s - 1);
        StlVecType<size_type>                       rand_indices(n_rows);

        for (size_type i = 0; i < n_rows; ++i)
            rand_indices[i] = dis(gen);
        std::sort(rand_indices.begin(), rand_indices.end());

        using TheView = ConstPtrView;

        typename TheView::IndexVecType  new_index;
        size_type                       prev_value;

        new_index.reserve(n_rows);
        for (size_type i = 0; i < n_rows; ++i)  {
            if (i == 0 || rand_indices[i] != prev_value)
                new_index.push_back(&(indices_[rand_indices[i]]));
            prev_value = rand_indices[i];
        }

        TheView dfv;

        dfv.indices_ = std::move(new_index);

        const SpinGuard guard(lock_);

        for (const auto &iter : column_list_)  {
            random_load_view_functor_<TheView, Ts ...>
                functor (iter.first.c_str(), rand_indices, dfv);

            data_[iter.second].change(functor);
        }

        return (dfv);
    }

    char buffer [512];

    snprintf (buffer, sizeof(buffer) - 1,
              "DataFrame::get_view_by_rand(): ERROR: "
#ifdef _MSC_VER
              "Number of rows requested %zu is more than available rows %zu",
#else
              "Number of rows requested %lu is more than available rows %lu",
#endif // _MSC_VER
              n_rows, index_s);
    throw BadRange (buffer);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<I, H> DataFrame<I, H>::
get_data(const StlVecType<const char *> &col_names) const  {

    DataFrame   df;

    df.load_index(indices_.begin(), indices_.end());

    const SpinGuard guard(lock_);

    for (const auto &name_citer : col_names)  {
        const auto  citer = column_tb_.find (name_citer);

        if (citer == column_tb_.end())  {
            char buffer [512];

            snprintf(buffer, sizeof(buffer) - 1,
                     "DataFrame::get_data(): ERROR: Cannot find column '%s'",
                     name_citer);
            throw ColNotFound(buffer);
        }

        load_all_functor_<Ts ...>   functor (name_citer, df);

        data_[citer->second].change(functor);
    }

    return (df);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::View DataFrame<I, H>::
get_view(const StlVecType<const char *> &col_names)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view()");

    View            dfv;
    const size_type idx_size = indices_.size();

    dfv.indices_ =
        typename View::IndexVecType(&(indices_[0]), &(indices_[0]) + idx_size);

    const SpinGuard guard(lock_);

    for (const auto &name_citer : col_names)  {
        const auto  citer = column_tb_.find (name_citer);

        if (citer == column_tb_.end())  {
            char buffer [512];

            snprintf(buffer, sizeof(buffer) - 1,
                     "DataFrame::get_view(): ERROR: Cannot find column '%s'",
                     name_citer);
            throw ColNotFound(buffer);
        }

        view_setup_functor_<View, Ts ...>   functor (citer->first.c_str(),
                                                     0,
                                                     idx_size,
                                                     dfv);

        data_[citer->second].change(functor);
    }

    return (dfv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::ConstView
DataFrame<I, H>::
get_view(const StlVecType<const char *> &col_names) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_view()");

    ConstView       dfcv;
    const size_type idx_size = indices_.size();

    dfcv.indices_ =
        typename ConstView::IndexVecType(&(indices_[0]),
                                         &(indices_[0]) + idx_size);

    const SpinGuard guard(lock_);

    for (const auto &name_citer : col_names)  {
        const auto  citer = column_tb_.find (name_citer);

        if (citer == column_tb_.end())  {
            char buffer [512];

            snprintf(buffer, sizeof(buffer) - 1,
                     "DataFrame::get_view(): ERROR: Cannot find column '%s'",
                     name_citer);
            throw ColNotFound(buffer);
        }

        view_setup_functor_<ConstView, Ts ...>  functor (citer->first.c_str(),
                                                         0,
                                                         idx_size,
                                                         dfcv);

        data_[citer->second].change(functor);
    }

    return (dfcv);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename ... Ts>
DataFrame<T, H> DataFrame<I, H>::
get_reindexed(const char *col_to_be_index, const char *old_index_name) const  {

    DataFrame<T, HeteroVector<align_value>> result;
    const auto                              &new_idx =
        get_column<T>(col_to_be_index);
    const size_type                         new_idx_s =
        result.load_index(new_idx.begin(), new_idx.end());

    if (old_index_name)  {
        const auto      &curr_idx = get_index();
        const size_type col_s =
            curr_idx.size() >= new_idx_s ? new_idx_s : curr_idx.size();

        result.template load_column<IndexType>(
            old_index_name, { curr_idx.begin(), curr_idx.begin() + col_s });
    }

    const SpinGuard guard(lock_);

    for (const auto &citer : column_list_)  {
        if (citer.first == col_to_be_index)  continue;

        load_functor_<DataFrame<T, HeteroVector<align_value>>, Ts ...>
            functor (citer.first.c_str(),
                     0,
                     new_idx_s,
                     result,
                     nan_policy::dont_pad_with_nans);

        data_[citer.second].change(functor);
    }

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename ... Ts>
typename DataFrame<T, H>::View DataFrame<I, H>::
get_reindexed_view(const char *col_to_be_index, const char *old_index_name)  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_reindexed_view()");

    using TheView = typename DataFrame<T, H>::View;

    TheView         result;
    auto            &new_idx = get_column<T>(col_to_be_index);
    const size_type new_idx_s = new_idx.size();

    result.indices_ = typename TheView::IndexVecType();
    result.indices_.set_begin_end_special(&(new_idx.front()),
                                          &(new_idx.back()));
    if (old_index_name)  {
        auto            &curr_idx = get_index();
        const size_type col_s =
            curr_idx.size() >= new_idx_s ? new_idx_s : curr_idx.size();

        result.template setup_view_column_<IndexType,
                                           typename IndexVecType::iterator>
            (old_index_name, { curr_idx.begin(), curr_idx.begin() + col_s });
    }

    const SpinGuard guard(lock_);

    for (const auto &citer : column_list_)  {
        if (citer.first == col_to_be_index)  continue;

        view_setup_functor_<TheView, Ts ...>    functor (
            citer.first.c_str(),
            0,
            new_idx_s,
            result);

        data_[citer.second].change(functor);
    }

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename ... Ts>
typename DataFrame<T, H>::ConstView
DataFrame<I, H>::
get_reindexed_view(const char *col_to_be_index,
                   const char *old_index_name) const  {

    static_assert(std::is_base_of<HeteroVector<align_value>, H>::value,
                  "Only a StdDataFrame can call get_reindexed_view()");

    using TheView = typename DataFrame<T, H>::ConstView;

    TheView         result;
    const auto      &new_idx = get_column<T>(col_to_be_index);
    const size_type new_idx_s = new_idx.size();

    result.indices_ = typename TheView::IndexVecType();
    result.indices_.set_begin_end_special(&(new_idx.front()),
                                          &(new_idx.back()));
    if (old_index_name)  {
        auto            &curr_idx = get_index();
        const size_type col_s =
            curr_idx.size() >= new_idx_s ? new_idx_s : curr_idx.size();

        result.template setup_view_column_<
                IndexType,
                typename IndexVecType::const_iterator>
            (old_index_name, { curr_idx.begin(), curr_idx.begin() + col_s });
    }

    const SpinGuard guard(lock_);

    for (const auto &citer : column_list_)  {
        if (citer.first == col_to_be_index)  continue;

        view_setup_functor_<TheView, Ts ...>    functor (
            citer.first.c_str(),
            0,
            new_idx_s,
            result);

        data_[citer.second].change(functor);
    }

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
typename DataFrame<I, H>::template StlVecType<
    std::tuple<typename DataFrame<I, H>::ColNameType,
               typename DataFrame<I, H>::size_type,
               std::type_index>>
DataFrame<I, H>::get_columns_info () const  {

    StlVecType<std::tuple<ColNameType, size_type, std::type_index>> result;

    result.reserve(column_list_.size());

    const SpinGuard guard(lock_);

    for (const auto &citer : column_list_)  {
        columns_info_functor_<Ts ...>   functor (result, citer.first.c_str());

        data_[citer.second].change(functor);
    }

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename ... Ts>
DataFrame<std::string, H>
DataFrame<I, H>::describe() const  {

    DataFrame<std::string, HeteroVector<align_value>>   result;

    result.load_index(describe_index_col.begin(), describe_index_col.end());

    const SpinGuard guard(lock_);

    for (const auto &citer : column_list_)  {
        describe_functor_<Ts ...>   functor (citer.first.c_str(), result);

        data_[citer.second].change(functor);
    }

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T>
bool DataFrame<I, H>::
pattern_match(const char *col_name,
              pattern_spec pattern,
              double epsilon) const  {

    const auto  &col = get_column<T>(col_name);

    switch(pattern)  {
    case pattern_spec::monotonic_increasing:
        return (is_monotonic_increasing(col));
    case pattern_spec::strictly_monotonic_increasing:
        return (is_strictly_monotonic_increasing(col));
    case pattern_spec::monotonic_decreasing:
        return (is_monotonic_decreasing(col));
    case pattern_spec::strictly_monotonic_decreasing:
        return (is_strictly_monotonic_decreasing(col));
    case pattern_spec::normally_distributed:
        return (is_normal(col, epsilon, false));
    case pattern_spec::standard_normally_distributed:
        return (is_normal(col, epsilon, true));
    case pattern_spec::lognormally_distributed:
        return (is_lognormal(col, epsilon));
    default:
        throw NotImplemented("pattern_match(): "
                             "Requested pattern is not implemented");
    }
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename DF, typename F>
typename DataFrame<I, H>::template StlVecType<T> DataFrame<I, H>::
combine(const char *col_name, const DF &rhs, F &functor) const  {

    SpinGuard   guard (lock_);
    const auto  &lhs_col = get_column<T>(col_name, false);
    const auto  &rhs_col = rhs.template get_column<T>(col_name, false);

    guard.release();

    const size_type col_s = std::min(lhs_col.size(), rhs_col.size());
    StlVecType<T>   result;

    result.reserve(col_s);
    for (size_type i = 0; i < col_s; ++i)
        result.push_back(std::move(functor(lhs_col[i], rhs_col[i])));

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename DF1, typename DF2, typename F>
typename DataFrame<I, H>::template StlVecType<T> DataFrame<I, H>::
combine(const char *col_name,
        const DF1 &df1,
        const DF2 &df2,
        F &functor) const  {

    SpinGuard   guard (lock_);
    const auto  &lhs_col = get_column<T>(col_name, false);
    const auto  &df1_col = df1.template get_column<T>(col_name, false);
    const auto  &df2_col = df2.template get_column<T>(col_name, false);

    guard.release();

    const size_type col_s =
        std::min<size_type>({ lhs_col.size(), df1_col.size(), df2_col.size() });
    StlVecType<T>   result;

    result.reserve(col_s);
    for (size_type i = 0; i < col_s; ++i)
        result.push_back(
            std::move(functor(lhs_col[i], df1_col[i], df2_col[i])));

    return (result);
}

// ----------------------------------------------------------------------------

template<typename I, typename H>
template<typename T, typename DF1, typename DF2, typename DF3, typename F>
typename DataFrame<I, H>::template StlVecType<T> DataFrame<I, H>::
combine(const char *col_name,
        const DF1 &df1,
        const DF2 &df2,
        const DF3 &df3,
        F &functor) const  {

    SpinGuard   guard (lock_);
    const auto  &lhs_col = get_column<T>(col_name, false);
    const auto  &df1_col = df1.template get_column<T>(col_name, false);
    const auto  &df2_col = df2.template get_column<T>(col_name, false);
    const auto  &df3_col = df3.template get_column<T>(col_name, false);

    guard.release();

    const size_type col_s = std::min<size_type>(
        { lhs_col.size(), df1_col.size(), df2_col.size(), df3_col.size() });
    StlVecType<T>   result;

    result.reserve(col_s);
    for (size_type i = 0; i < col_s; ++i)
        result.push_back(
            std::move(functor(lhs_col[i], df1_col[i], df2_col[i], df3_col[i])));

    return (result);
}

} // namespace hmdf

// ----------------------------------------------------------------------------

// Local Variables:
// mode:C++
// tab-width:4
// c-basic-offset:4
// End:
