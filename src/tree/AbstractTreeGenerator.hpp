/*
This file is a part of FAMSA software distributed under GNU GPL 3 licence.
The homepage of the FAMSA project is http://sun.aei.polsl.pl/REFRESH/famsa

Authors: Sebastian Deorowicz, Agnieszka Debudaj-Grabysz, Adam Gudys

*/
#pragma once
#include "AbstractTreeGenerator.h"

#include "../lcs/lcsbp.h"

#include <cmath>
#include <type_traits>

// overloads for converting sequence type to pointer
inline CSequence* seq_to_ptr(CSequence* x) { return x; }
inline CSequence* seq_to_ptr(CSequence& x) { return &x; }

// dummy implementation
template <class T, Measure measure>
struct Transform {
	//static_assert(0, "Cannot use dummy implementation");
	
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) {
		return 0;
	}
};

template <class T>
struct Transform<T, Measure::SimilarityDefault>{
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) { 
		T indel = len1 + len2 - 2 * lcs;
		return indel == 0 ? ((T)lcs * 1000) : ((T)lcs / indel); 
	}
};

template <class T>
struct Transform<T, Measure::LCS2_AB>{
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) { 
		return (T)lcs * (T)(lcs) / ((T)len1 * (T)len2);
	}
};

template <class T>
struct Transform<T, Measure::LCS2_indel_ApB>{
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) { 
		T indel = len1 + len2 - 2 * lcs;
		T s = len1 + len2;
		T l = lcs;
		if (indel == 0)
			return 100000000;
//		return l * l / (4*s*s + 4*l*l);*/

/*		double m = log2(log2(min(len1, len2)));
		double M = log2(log2(max(len1, len2)));

		return (l / indel) * (m / M);*/

/*		T s_geo = sqrt((T)len1 * (T)len2);
		T indel_geo = 2 * s_geo - 2 * lcs;
		if (indel_geo < 0.0001)
			indel_geo = 0.0001;

		return l / sqrt(indel_geo);*/

		T s_harm = 1.0 / (1.0 / (T)len1 + 1.0 / (T)len2);
		T indel_harm = 2 * s_harm - 2 * lcs;
		if (indel_harm < 0.0001)
			indel_harm = 0.0001;

		return l / sqrt(indel_harm);

//		return l / sqrt(indel);
//		return sqrt(l) / log2(indel);
	}
};

template <class T>
struct Transform<T, Measure::LCS_sqrt_indel>{
private:
	std::vector<T> pp_sqrt_rec;
	uint32_t cur_pp_size = 0;

	void pp_extend(uint32_t val)
	{
		pp_sqrt_rec.resize(val + 1);
		for (; cur_pp_size <= val; ++cur_pp_size)
			pp_sqrt_rec[cur_pp_size] = 1.0 / sqrt(cur_pp_size);
//			pp_sqrt_rec[cur_pp_size] = 1.0 / pow(cur_pp_size, 1.0 / 3.0);
	}

public:
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) { 
		T indel = len1 + len2 - 2 * lcs;
		T l = lcs;

		if (indel == 0)
			return 100000000;

		if (indel >= cur_pp_size)
			pp_extend(indel);

/*		T m = min(len1, len2);
		T M = max(len1, len2);
		T f = pow(m / M, 0.01);

		return l * pp_sqrt_rec[indel] * f;*/
		return l * pp_sqrt_rec[indel];
	}
};

template <class T>
struct Transform<T, Measure::DistanceReciprocal> {
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) {
		T indel = len1 + len2 - 2 * lcs;
		return (T)indel / lcs; 
	}
};

template <class T>
struct Transform<T, Measure::DistanceInverse> {
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) {
		T indel = len1 + len2 - 2 * lcs;
		return indel == 0 ? (-(T)lcs * 1000) : (-(T)lcs / indel); 
	}
};

template <class T>
struct Transform<T, Measure::DistanceLCSByLength> {
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) {
		return 1.0 - (T)lcs / std::min(len1, len2);
	}
};

template <class T>
struct Transform<T, Measure::DistanceLCSByLengthCorrected> {
	T operator()(uint32_t lcs, uint32_t len1, uint32_t len2) {
		// make len1 the longer 
		if (len1 < len2) { std::swap(len1, len2);  }
		
		T d = 1.0 - (T)lcs / len2;

		// MAFFT distance correction
		auto correction = [](T x, T y) ->T { return y / x * 0.1 + 10000 / (x + 10000) + 0.01;  };
		d = d / correction(len1, len2);
		return d;
	}
};

// *******************************************************************
/*
similarity_type can be:
	- CSequence,
	- CSequence*,
*/
template <class seq_type, class similarity_type, typename Transform>
void AbstractTreeGenerator::calculateSimilarityVector(
	Transform& transform,
	seq_type& ref,
	seq_type* sequences, 
	size_t n_seqs, 
	similarity_type* out_vector, 
	CLCSBP& lcsbp)
{
	uint32_t lcs_lens[4];
	
	seq_to_ptr(ref)->ComputeBitMasks();

	// process portions of 4 sequences
	for (int j = 0; j < n_seqs / 4; ++j) {
		lcsbp.GetLCSBP(
			seq_to_ptr(ref),
			seq_to_ptr(sequences[j * 4 + 0]),
			seq_to_ptr(sequences[j * 4 + 1]),
			seq_to_ptr(sequences[j * 4 + 2]),
			seq_to_ptr(sequences[j * 4 + 3]),
			lcs_lens);

		for (int k = 0; k < 4; ++k) {
			out_vector[j * 4 + k] = transform(lcs_lens[k], seq_to_ptr(ref)->length, seq_to_ptr(sequences[j * 4 + k])->length);
		}
	}

	// if there is something left
	size_t n_processed = n_seqs / 4 * 4;
	if (n_processed < n_seqs) {
		lcsbp.GetLCSBP(
			seq_to_ptr(ref),
			(n_processed + 0 < n_seqs) ? seq_to_ptr(sequences[n_processed + 0]) : nullptr,
			(n_processed + 1 < n_seqs) ? seq_to_ptr(sequences[n_processed + 1]) : nullptr,
			(n_processed + 2 < n_seqs) ? seq_to_ptr(sequences[n_processed + 2]) : nullptr,
			(n_processed + 3 < n_seqs) ? seq_to_ptr(sequences[n_processed + 3]) : nullptr,
			lcs_lens);

		for (int k = 0; k < 4 && n_processed + k < n_seqs; ++k)
			out_vector[n_processed + k] = transform(lcs_lens[k], seq_to_ptr(ref)->length, seq_to_ptr(sequences[n_processed + k])->length);
	}

	seq_to_ptr(ref)->ReleaseBitMasks();
}

// *******************************************************************
/*
similarity_type can be:
	- CSequence,
	- CSequence*,
*/
template <class seq_type, class similarity_type, typename Iter, typename Transform>
void AbstractTreeGenerator::calculateSimilarityRange(
	Transform &transform,
	seq_type& ref, 
	seq_type* sequences, 
	pair<Iter, Iter> ids_range,
	similarity_type* out_vector,
	CLCSBP& lcsbp)
{
	uint32_t lcs_lens[4];
	size_t n_seqs = distance(ids_range.first, ids_range.second);

	auto p_ids = ids_range.first;

	// process portions of 4 sequences
	for (int j = 0; j < n_seqs / 4; ++j, p_ids += 4) {
		lcsbp.GetLCSBP(
			seq_to_ptr(ref),
			seq_to_ptr(sequences[*(p_ids + 0)]),
			seq_to_ptr(sequences[*(p_ids + 1)]),
			seq_to_ptr(sequences[*(p_ids + 2)]),
			seq_to_ptr(sequences[*(p_ids + 3)]),
			lcs_lens);

		for (int k = 0; k < 4; ++k) {
			out_vector[j * 4 + k] = transform(lcs_lens[k], seq_to_ptr(ref)->length, seq_to_ptr(sequences[*(p_ids + k)])->length);
		}
	}

	// if there is something left
	size_t n_processed = n_seqs / 4 * 4;
	if (n_processed < n_seqs) {
		lcsbp.GetLCSBP(
			seq_to_ptr(ref),
/*			(n_processed + 0 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 0)]) : nullptr,
			(n_processed + 1 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 1)]) : nullptr,
			(n_processed + 2 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 2)]) : nullptr,
			(n_processed + 3 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 3)]) : nullptr,*/
			seq_to_ptr(sequences[*(p_ids + 0)]),
			(n_processed + 1 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 1)]) : seq_to_ptr(sequences[*(p_ids + 0)]),
			(n_processed + 2 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 2)]) : seq_to_ptr(sequences[*(p_ids + 0)]),
			(n_processed + 3 < n_seqs) ? seq_to_ptr(sequences[*(p_ids + 3)]) : seq_to_ptr(sequences[*(p_ids + 0)]),
			lcs_lens);

		for (int k = 0; k < 4 && n_processed + k < n_seqs; ++k)
			out_vector[n_processed + k] = transform(lcs_lens[k], seq_to_ptr(ref)->length, seq_to_ptr(sequences[*(p_ids + k)])->length);
	}
}


// *******************************************************************
template <class seq_type, class similarity_type, typename Transform>
void AbstractTreeGenerator::calculateSimilarityMatrix(
	Transform& transform,
	seq_type* sequences,
	size_t n_seq, 
	similarity_type *out_matrix, 
	CLCSBP& lcsbp) {

	for (size_t row_id = 0; row_id < n_seq; ++row_id) {
		
		size_t row_offset = TriangleMatrix::access(row_id, 0);
		
		calculateSimilarityVector<seq_type, similarity_type, decltype(transform)>(
			transform,
			sequences[row_id],
			sequences,
			row_id,
			out_matrix + row_offset,
			lcsbp);
	}	
}