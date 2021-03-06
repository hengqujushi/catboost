#pragma once

#include <cmath>

#include <catboost/cuda/cuda_lib/helpers.h>
#include <catboost/libs/helpers/exception.h>
#include <catboost/libs/data/load_data.h>
#include <catboost/libs/options/enums.h>

#include <library/grid_creator/binarization.h>
#include <library/threading/local_executor/local_executor.h>

#include <util/system/types.h>
#include <util/generic/yexception.h>
#include <util/string/builder.h>
#include <util/generic/ymath.h>

inline ui32 IntLog2(ui32 values) {
    return (ui32)ceil(log2(values));
}

template <class TStorageType>
class TIndexHelper {
public:
    explicit TIndexHelper(ui32 bitsPerKey)
        : BitsPerKey(bitsPerKey)
    {
        CB_ENSURE(bitsPerKey < 32, "Too many bits in key");
        EntriesPerType = sizeof(TStorageType) * CHAR_BIT / BitsPerKey;
    }

    inline ui64 Mask() const {
        return ((static_cast<TStorageType>(1) << BitsPerKey) - 1);
    }

    inline ui32 Offset(ui32 index) const {
        return index / EntriesPerType;
    }

    inline ui32 Shift(ui32 index) const {
        return (EntriesPerType - index % EntriesPerType - 1) * BitsPerKey;
    }

    inline ui32 GetBitsPerKey() const {
        return BitsPerKey;
    }

    inline ui32 GetEntriesPerType() const {
        return EntriesPerType;
    }

    inline ui32 CompressedSize(ui32 size) const {
        return (NHelpers::CeilDivide(size, EntriesPerType));
    }

    inline ui32 Extract(const TVector<ui64>& compressedData, ui32 index) const {
        const ui32 offset = Offset(index);
        const ui32 shift = Shift(index);
        return (compressedData[offset] >> shift) & Mask();
    }

    template <class T>
    inline void Write(TVector<ui64>& compressedData, ui32 index, T data) const {
        const ui32 offset = Offset(index);
        const ui32 shift = Shift(index);
        CB_ENSURE((data & Mask()) == data);
        compressedData[offset] = ((ui64)data << shift);
    }

private:
    ui32 BitsPerKey;
    ui32 EntriesPerType;
};

template <class TStorageType, class T>
inline TVector<TStorageType> CompressVector(const T* data, ui32 size, ui32 bitsPerKey) {
    CB_ENSURE(bitsPerKey < 32);
    CB_ENSURE(bitsPerKey, "Error: data with zero bits per key. Something went wrong");

    TVector<TStorageType> dst;
    TIndexHelper<TStorageType> indexHelper(bitsPerKey);
    dst.resize(indexHelper.CompressedSize(size));
    const auto mask = indexHelper.Mask();

    NPar::TLocalExecutor::TExecRangeParams params(0, size);
    //alignment by entries per int allows parallel compression
    params.SetBlockSize(indexHelper.GetEntriesPerType() * 8192);

    NPar::LocalExecutor().ExecRange([&](int blockIdx) {
        NPar::LocalExecutor().BlockedLoopBody(params, [&](int i) {
            const ui32 offset = indexHelper.Offset((ui32)i);
            const ui32 shift = indexHelper.Shift((ui32)i);
            CB_ENSURE((data[i] & mask) == data[i], TStringBuilder() << "Error: key contains too many bits: max bits per key: allowed " << bitsPerKey << ", observe key" << data[i]);
            dst[offset] |= static_cast<ui64>(data[i]) << shift;
        })(blockIdx);
    }, 0, params.GetBlockCount(), NPar::TLocalExecutor::WAIT_COMPLETE);

    return dst;
}

template <class TStorageType, class T>
inline TVector<TStorageType> CompressVector(const TVector<T>& data, ui32 bitsPerKey) {
    return CompressVector<TStorageType, T>(data.data(), data.size(), bitsPerKey);
}

template <class TStorageType, class T>
inline TVector<T> DecompressVector(const TVector<TStorageType>& compressedData, ui32 keys, ui32 bitsPerKey) {
    TVector<T> dst;
    CB_ENSURE(bitsPerKey < 32);
    CB_ENSURE(sizeof(T) <= sizeof(TStorageType));
    dst.clear();
    dst.resize(keys);
    const TIndexHelper<TStorageType> indexHelper(bitsPerKey);
    const auto mask = indexHelper.Mask();

    NPar::ParallelFor(0, keys, [&](int i) {
        const ui32 offset = indexHelper.Offset(i);
        const ui32 shift = indexHelper.Shift(i);
        dst[i] = (compressedData[offset] >> shift) & mask;
    });

    return dst;
}

template <class TBinType>
inline TBinType Binarize(const TVector<float>& borders,
                         float value) {
    ui32 index = 0;
    while (index < borders.size() && value > borders[index])
        ++index;

    TBinType resultIndex = static_cast<TBinType>(index);
    if (static_cast<ui32>(resultIndex) != resultIndex) {
        ythrow yexception() << "Error: can't binarize to binType for border count " << borders.size();
    }
    return index;
}

//this routine assumes NanMode = Min/Max means we have nans in float-values
template <class TBinType = ui32>
inline TVector<TBinType> BinarizeLine(const float* values,
                                      const ui64 valuesCount,
                                      const ENanMode nanMode,
                                      const TVector<float>& borders) {
    TVector<TBinType> result(valuesCount);

    NPar::TLocalExecutor::TExecRangeParams params(0, (int)valuesCount);
    params.SetBlockSize(16384);

    NPar::LocalExecutor().ExecRange([&](int blockIdx) {
        NPar::LocalExecutor().BlockedLoopBody(params, [&](int i) {
            float value = values[i];
            if (IsNan(value)) {
                CB_ENSURE(nanMode != ENanMode::Forbidden, "Error, NaNs for current feature are forbidden. (All NaNs are forbidden or test has NaNs and learn not)");
                result[i] = nanMode == ENanMode::Min ? 0 : borders.size();
            } else {
                ui32 bin =  Binarize<TBinType>(borders, values[i]);
                if (nanMode == ENanMode::Min) {
                    result[i] = bin + 1;
                } else {
                    result[i] = bin;
                }
            }
        })(blockIdx);
    }, 0, params.GetBlockCount(), NPar::TLocalExecutor::WAIT_COMPLETE);

    return result;
}

inline int StringToIntHash(const TStringBuf& buf) {
    return CalcCatFeatureHash(buf);
}
