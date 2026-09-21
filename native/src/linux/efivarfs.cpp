#include "efivarfs.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace langbios {

namespace {

const char* kEfiVarsDir = "/sys/firmware/efi/efivars/";

std::string PathFor(const std::string& name, const std::string& guid) {
    return kEfiVarsDir + name + "-" + guid;
}

std::string ErrnoMessage(const std::string& what) {
    return what + ": " + std::strerror(errno);
}

// Clears (or restores) the immutable inode flag efivarfs sets on these
// files by default, mirroring what efibootmgr itself does - a plain
// write() against an immutable file fails with EPERM even as root.
void SetImmutable(int fd, bool immutable) {
    int flags = 0;
    if (ioctl(fd, FS_IOC_GETFLAGS, &flags) != 0) return; // best-effort
    if (immutable) {
        flags |= FS_IMMUTABLE_FL;
    } else {
        flags &= ~FS_IMMUTABLE_FL;
    }
    ioctl(fd, FS_IOC_SETFLAGS, &flags); // best-effort; write attempt below is the real gate
}

} // namespace

bool EfiVarFsAvailable() {
    struct stat st{};
    return stat(kEfiVarsDir, &st) == 0;
}

std::vector<uint8_t> ReadEfiVariable(const std::string& name, const std::string& guid) {
    std::string path = PathFor(name, guid);
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error(ErrnoMessage("Could not open " + path));
    }

    struct stat st{};
    if (fstat(fd, &st) != 0) {
        close(fd);
        throw std::runtime_error(ErrnoMessage("fstat failed on " + path));
    }

    std::vector<uint8_t> buf(st.st_size);
    ssize_t n = read(fd, buf.data(), buf.size());
    close(fd);
    if (n < 0) {
        throw std::runtime_error(ErrnoMessage("Read failed on " + path));
    }
    buf.resize(n);

    if (buf.size() < 4) {
        throw std::runtime_error(path + " is malformed (shorter than the 4-byte attribute header)");
    }
    // First 4 bytes are the little-endian attribute flags; the rest is data.
    return std::vector<uint8_t>(buf.begin() + 4, buf.end());
}

void WriteEfiVariable(const std::string& name, const std::string& guid,
                      uint32_t attributes, const std::vector<uint8_t>& data) {
    std::string path = PathFor(name, guid);

    std::vector<uint8_t> full(4 + data.size());
    full[0] = static_cast<uint8_t>(attributes & 0xFF);
    full[1] = static_cast<uint8_t>((attributes >> 8) & 0xFF);
    full[2] = static_cast<uint8_t>((attributes >> 16) & 0xFF);
    full[3] = static_cast<uint8_t>((attributes >> 24) & 0xFF);
    std::copy(data.begin(), data.end(), full.begin() + 4);

    int fdFlags = open(path.c_str(), O_RDONLY);
    bool wasImmutable = false;
    if (fdFlags >= 0) {
        int flags = 0;
        if (ioctl(fdFlags, FS_IOC_GETFLAGS, &flags) == 0) {
            wasImmutable = (flags & FS_IMMUTABLE_FL) != 0;
        }
        if (wasImmutable) SetImmutable(fdFlags, false);
        close(fdFlags);
    }

    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0) {
        throw std::runtime_error(ErrnoMessage(
            "Could not open " + path + " for writing (needs root - CAP_SYS_ADMIN)"));
    }
    ssize_t written = write(fd, full.data(), full.size());
    int writeErrno = errno;
    close(fd);

    if (written < 0 || static_cast<size_t>(written) != full.size()) {
        throw std::runtime_error(
            path + ": write failed (" + std::strerror(writeErrno) +
            "). efivarfs requires the whole new value in a single write() call.");
    }

    if (wasImmutable) {
        int fd2 = open(path.c_str(), O_RDONLY);
        if (fd2 >= 0) {
            SetImmutable(fd2, true);
            close(fd2);
        }
    }
}

} // namespace langbios
