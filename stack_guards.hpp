#pragma once
#include <deque>
#include <string>

// Relatively lightweight stack trace implementation
// (Presumably) zero overhead when no exceptions are thrown

class StackableException : public std::exception
{
    mutable std::deque<std::string> trace;
    mutable std::string text;

  public:
    StackableException(
        const std::string &message,
        const std::string &function,
        const std::string &file,
        int line)
    {
        text.append(message + " in " + function + "() at " + file + ':' + std::to_string(line));
    }

    // const is a lie
    void push(const std::string &function, const std::string &file, int line) const
    {
        text.append("\n -- caught in " + function + "() at " + file + ':' + std::to_string(line));
    }

    const char *what() const noexcept override
    {
        return text.c_str();
    }
};

#define EXCEPTION(message) StackableException(            \
    std::string("StackableException '") + message + '\'', \
    __FUNCTION__,                                         \
    __FILE__,                                             \
    __LINE__)

#define BEGIN() \
    try         \
    {

#define END()                                                   \
    }                                                           \
    catch (const StackableException &ex)                        \
    {                                                           \
        ex.push(__FUNCTION__, __FILE__, __LINE__);              \
        throw;                                                  \
    }                                                           \
    catch (const std::exception &ex)                            \
    {                                                           \
        StackableException stex(                                \
            std::string("std::exception '") + ex.what() + '\'', \
            __FUNCTION__,                                       \
            __FILE__,                                           \
            __LINE__);                                          \
        throw stex;                                             \
    }

#define END_AND_CATCH(ex) \
    }                       \
    catch (const std::exception &ex)
