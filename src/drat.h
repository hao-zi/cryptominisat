/******************************************
Copyright (c) 2016, Mate Soos

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
***********************************************/

#ifndef __DRAT_H__
#define __DRAT_H__

#include "clause.h"
#include <iostream>

namespace CMSat {

enum DratFlag{fin, deldelay, del, findelay, add};

struct Drat
{
    Drat()
    {
    }

    virtual ~Drat()
    {
    }

    virtual bool enabled()
    {
        return false;
    }

    virtual void forget_delay()
    {
    }

    virtual bool get_conf_id() {
        return false;
    }

    virtual bool something_delayed()
    {
        return false;
    }

    virtual Drat& operator<<(const Lit)
    {

        return *this;
    }

    virtual Drat& operator<<(const Clause&)
    {
        return *this;
    }

    virtual Drat& operator<<(const vector<Lit>&)
    {
        return *this;
    }

    virtual Drat& operator<<(const DratFlag)
    {
        return *this;
    }

    virtual void setFile(std::ostream*)
    {
    }

    int buf_len = 0;
    unsigned char* drup_buf = 0;
    unsigned char* buf_ptr;
};

template<bool add_ID>
struct DratFile: public Drat
{
    DratFile()
    {
        drup_buf = new unsigned char[2 * 1024 * 1024];
        buf_ptr = drup_buf;

        del_buf = new unsigned char[2 * 1024 * 1024];
        del_ptr = del_buf;
    }

    virtual ~DratFile()
    {
        delete drup_buf;
        delete del_buf;
    }

    void byteDRUPa(const Lit l)
    {
        unsigned int u = 2 * (l.var() + 1) + l.sign();
        do {
            *buf_ptr++ = u & 0x7f | 0x80;
            buf_len++;
            u = u >> 7;
        } while (u);

        // End marker of this unsigned number
        *(buf_ptr - 1) &= 0x7f;
    }

    void byteDRUPd(const Lit l)
    {
        unsigned int u = 2 * (l.var() + 1) + l.sign();
        do {
            *del_ptr++ = u & 0x7f | 0x80;
            del_len++;
            u = u >> 7;
        } while (u);

        // End marker of this unsigned number
        *(del_ptr - 1) &= 0x7f;
    }

    void binDRUP_flush() {
        //fwrite_unlocked(drup_buf, sizeof(unsigned char), buf_len, drup_file);
        drup_file->write((const char*)drup_buf, buf_len);
        buf_ptr = drup_buf;
        buf_len = 0;
    }

    void setFile(std::ostream* _file) override
    {
        drup_file = _file;
    }

    bool get_conf_id() override {
        return add_ID;
    }

    bool something_delayed() override
    {
        return delete_filled;
    }

    void forget_delay() override
    {
        del_ptr = del_buf;
        must_delete_next = false;
        delete_filled = false;
    }

    bool enabled() override
    {
        return true;
    }

    int del_len = 0;
    unsigned char* del_buf;
    unsigned char* del_ptr;

    bool delete_filled = false;
    bool must_delete_next = false;

    Drat& operator<<(const Lit lit) override
    {
        if (must_delete_next) {
            byteDRUPd(lit);
        } else {
            byteDRUPa(lit);
        }

        return *this;
    }

    Drat& operator<<(const Clause& cl) override
    {
        if (must_delete_next) {
            for(const Lit l: cl)
                byteDRUPd(l);
        } else {
            for(const Lit l: cl)
                byteDRUPa(l);
        }
        #ifdef STATS_NEEDED
        if (add_ID) {
            ID = cl.stats.ID;
            assert(ID != 0);
        }
        #endif

        return *this;
    }

    Drat& operator<<(const DratFlag flag) override
    {
        switch (flag)
        {
            case DratFlag::fin:
                if (must_delete_next) {
                    *del_ptr++ = 0;
                    del_len++;
                    delete_filled = true;
                } else {
                    *buf_ptr++ = 0;
                    buf_len++;
                    if (add_ID) {
                        //HACK and *will not work*, 31b is too small!!
                        byteDRUPa(Lit(ID, false));
                    }
                    if (buf_len > 1048576) {
                        binDRUP_flush();
                    }
                }
                if (add_ID) {
                    ID = 1;
                }
                must_delete_next = false;
                break;

            case DratFlag::deldelay:
                assert(!delete_filled);
                del_ptr = del_buf;
                del_len = 0;
                *buf_ptr++ = 'd';
                buf_len++;
                delete_filled = false;

                must_delete_next = true;
                break;

            case DratFlag::findelay:
                assert(delete_filled);
                memcpy(buf_ptr, del_buf, del_len);
                buf_len += del_len;
                buf_ptr += del_len;
                if (buf_len > 1048576) {
                    binDRUP_flush();
                }

                del_ptr = del_buf;
                del_len = 0;
                delete_filled = false;
                break;

            case DratFlag::add:
                *buf_ptr++ = 'a';
                buf_len++;
                break;

            case DratFlag::del:
                del_ptr = del_buf;
                del_len = 0;
                delete_filled = false;

                must_delete_next = false;
                *buf_ptr++ = 'd';
                buf_len++;
                break;
        }

        return *this;
    }

    /*Drat& operator<<(const vector<Lit>& lits) override
    {
        if (must_delete_next) {
            todel << lits << " ";
        } else {
            *file << lits << " ";
        }

        return *this;
    }*/

    std::ostream* drup_file = NULL;
    int64_t ID = 1;
};

}

#endif //__DRAT_H__
