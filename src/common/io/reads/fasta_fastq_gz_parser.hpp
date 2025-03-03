//***************************************************************************
//* Copyright (c) 2023-2024 SPAdes team
//* Copyright (c) 2015-2022 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once

#include "single_read.hpp"

#include "utils/verify.hpp"
#include "io/reads/parser.hpp"
#include "sequence/quality.hpp"
#include "sequence/nucl.hpp"

#include "kseq/kseq.h"

#include <zlib.h>
#include <string>

namespace io {

namespace fastafastqgz {
// STEP 1: declare the type of file handler and the read() function
// Silence bogus gcc warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
// STEP 1: declare the type of file handler and the read() function
KSEQ_INIT(gzFile, gzread)
#pragma GCC diagnostic pop
}

class FastaFastqGzParser: public Parser {
public:
    /*
     * Default constructor.
     *
     * @param filename The name of the file to be opened.
     * @param offset The offset of the read quality.
     */
    FastaFastqGzParser(const std::filesystem::path& filename,
                       FileReadFlags flags = FileReadFlags())
            : Parser(filename, flags), fp_(), seq_(NULL) {
        open();
    }

    /*
     * Default destructor.
     */
    /* virtual */
    ~FastaFastqGzParser() {
        close();
    }

    /*
     * Read SingleRead from stream.
     *
     * @param read The SingleRead that will store read data.
     *
     * @return Reference to this stream.
     */
    /* virtual */
    FastaFastqGzParser& operator>>(SingleRead& read) {
        if (!is_open_ || eof_)
            return *this;

        if (seq_->qual.s && flags_.use_name && flags_.use_quality) {
            read = SingleRead(seq_->name.s ? seq_->name.s : "",
                              seq_->comment.s ? seq_->comment.s : "",
                              seq_->seq.s, seq_->qual.s,
                              flags_.offset,
                              0, 0, flags_.validate);
        } else if (flags_.use_name && seq_->name.s) {
            read = SingleRead(seq_->name.s,
                              seq_->comment.s ? seq_->comment.s : "",
                              seq_->seq.s,
                              0, 0, flags_.validate);
        } else if (flags_.use_comment && seq_->comment.s) {
            read = SingleRead("", seq_->comment.s,
                              seq_->seq.s,
                              0, 0, flags_.validate);
        } else
            read = SingleRead(seq_->seq.s,
                              0, 0, flags_.validate);

        ReadAhead();
        return *this;
    }

    /*
     * Close the stream.
     */
    /* virtual */
    void close() {
        if (!is_open_)
            return;

        // STEP 5: destroy seq
        fastafastqgz::kseq_destroy(seq_);
        // STEP 6: close the file handler
        gzclose(fp_);
        is_open_ = false;
        eof_ = true;
    }

private:
    /*
     * @variable File that is associated with gzipped data file.
     */
    gzFile fp_;
    /*
     * @variable Data element that stores last SingleRead got from
     * stream.
     */
    fastafastqgz::kseq_t* seq_;

    /*
     * Open a stream.
     */
    /* virtual */
    void open() {
        // STEP 2: open the file handler
        fp_ = gzopen(filename_.c_str(), "r");
        if (!fp_) {
            is_open_ = false;
            return;
        }
        // STEP 3: initialize seq
        seq_ = fastafastqgz::kseq_init(fp_);
        eof_ = false;
        is_open_ = true;
        ReadAhead();
    }

    /*
     * Read next SingleRead from file.
     */
    void ReadAhead() {
        VERIFY(is_open_);
        VERIFY(!eof_);
        if (fastafastqgz::kseq_read(seq_) < 0) {
            eof_ = true;
        }
    }

    /*
     * Hidden copy constructor.
     */
    FastaFastqGzParser(const FastaFastqGzParser& parser);
    /*
     * Hidden assign operator.
     */
    void operator=(const FastaFastqGzParser& parser);
};

}
