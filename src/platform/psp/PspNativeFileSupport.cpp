/// <summary>
/// Retains the generated runtime file implementation used by native packaged asset loading when managed code does not reference System.IO.File.
/// </summary>
#include "helcpp_config.hpp"

// Avoid duplicate definitions when managed reachability already includes File.
#if !defined(HE_CPP_REQ_FILE)
#define HE_CPP_REQ_FILE 1
#include "system/io/file.cpp"
#endif
