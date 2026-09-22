// TPM on macOS: doesn't exist. Apple uses the Secure Enclave (SEP)
// instead - an entirely different, closed architecture with no public
// read/write API. This is a real, permanent platform difference, not
// a gap in this code - honestly reported rather than faked.
#include "langbios/tpm.hpp"

namespace langbios {

Result GetTpmState() {
    return Result::Failure(
        "tpm does not exist on macOS. Apple uses the Secure Enclave (SEP) "
        "instead - a different, closed architecture with no public "
        "read/write API, so there's nothing this tool (or any third-party "
        "software) can query here.");
}

} // namespace langbios
