#pragma once

#include "last_getopt_opts.h"
#include "last_getopt_easy_setup.h"
#include "last_getopt_parse_result.h"

#include <util/generic/function.h>

/// see some documentation in
/// https://wiki.yandex-team.ru/development/poisk/arcadia/util/lastgetopt/
/// https://wiki.yandex-team.ru/development/poisk/arcadia/library/getopt/
/// see examples in library/getopt/last_getopt_demo

//TODO: in most cases this include is unnecessary, but needed THandlerFunctor1<TpFunc, TpArg>::HandleOpt
#include "last_getopt_parser.h"

namespace NLastGetopt {

    /// Handler to split option value by delimiter into a target container and allow ranges.
    template <class Container>
    struct TOptRangeSplitHandler : public IOptHandler {
    public:
        using TContainer = Container;
        using TValue = typename TContainer::value_type;

        explicit TOptRangeSplitHandler(TContainer* target, const char elementsDelim, const char rangesDelim)
            : Target(target)
            , ElementsDelim(elementsDelim)
            , RangesDelim(rangesDelim)
        {
        }

        void HandleOpt(const TOptsParser* parser) override {
            const TStringBuf curval(parser->CurValOrDef());
            if (curval.IsInited()) {
                TConsumer cons = {parser->CurOpt(), Target, RangesDelim};
                SplitStringTo(curval, ElementsDelim, &cons);
            }
        }

    private:
        struct TConsumer {
            const TOpt* CurOpt;
            TContainer* Target;
            const char RangesDelim;

            typedef TStringBuf value_type;
            void push_back(const TStringBuf& val) {
                TStringBuf mutableValue = val;

                TValue first = NPrivate::OptFromString<TValue>(mutableValue.NextTok(RangesDelim), CurOpt);
                TValue last = mutableValue ? NPrivate::OptFromString<TValue>(mutableValue, CurOpt) : first;

                if (last < first) {
                    ythrow TUsageException() << "failed to parse opt " << NPrivate::OptToString(CurOpt) << " value " << TString(val).Quote() << ": the second argument is less than the first one";
                }

                for (++last; first < last; ++first) {
                    Target->insert(Target->end(), first);
                }
            }
        };

        TContainer* Target;
        char ElementsDelim;
        char RangesDelim;
    };

    template <class Container>
    struct TOptSplitHandler : public IOptHandler {
    public:
        using TContainer = Container;
        using TValue = typename TContainer::value_type;

        explicit TOptSplitHandler(TContainer* target, const char delim)
            : Target(target)
            , Delim(delim)
        {
        }

        void HandleOpt(const TOptsParser* parser) override {
            const TStringBuf curval(parser->CurValOrDef());
            if (curval.IsInited()) {
                TConsumer cons = {parser->CurOpt(), Target};
                SplitStringTo(curval, Delim, &cons);
            }
        }

    private:
        struct TConsumer {
            const TOpt* CurOpt;
            TContainer* Target;
            typedef TStringBuf value_type;
            void push_back(const TStringBuf& val) {
                Target->insert(Target->end(), NPrivate::OptFromString<TValue>(val, CurOpt));
            }
        };

        TContainer* Target;
        char Delim;
    };

    template <class TpFunc>
    struct TOptKVHandler : public IOptHandler {
    public:
        using TKey = typename TFunctionArgs<TpFunc>::template TGet<0>;
        using TValue = typename TFunctionArgs<TpFunc>::template TGet<1>;

        explicit TOptKVHandler(TpFunc func, const char kvdelim = '=')
            : Func(func)
            , KVDelim(kvdelim)
        {
        }

        void HandleOpt(const TOptsParser* parser) override {
            const TStringBuf curval(parser->CurValOrDef());
            const TOpt* curOpt(parser->CurOpt());
            if (curval.IsInited()) {
                TStringBuf key, value;
                if (!curval.TrySplit(KVDelim, key, value)) {
                    ythrow TUsageException() << "failed to parse opt " << NPrivate::OptToString(curOpt)
                            << " value " << TString(curval).Quote() << ": expected key" << KVDelim << "value format";
                }
                Func(NPrivate::OptFromString<TKey>(key, curOpt), NPrivate::OptFromString<TValue>(value, curOpt));
            }
        }

    private:
        TpFunc Func;
        char KVDelim;
    };

namespace NPrivate {

template <typename TpFunc, typename TpArg>
void THandlerFunctor1<TpFunc, TpArg>::HandleOpt(const TOptsParser* parser) {
    const TStringBuf curval = parser->CurValOrDef(!HasDef_);
    const TpArg& arg = curval.IsInited() ? OptFromString<TpArg>(curval, parser->CurOpt()) : Def_;
    try {
        Func_(arg);
    } catch (...) {
        ythrow TUsageException() << "failed to handle opt " << OptToString(parser->CurOpt())
            << " value " << TString(curval).Quote() << ": " << CurrentExceptionMessage();
    }
}

} // NPrivate

} // NLastGetopt
