#include <filesystem>

namespace fs = std::filesystem;

namespace directories
{
// Various mount directories shall be used to simulate different mount/unmount
// behavior:
// - mountDir0: failing mount for version 3.1.1
// - mountDir1: failing mount for all versions
// - mountDir2: failing unmount
// - mountDir3: correct mount and unmount
static const fs::path mountRoot{fs::temp_directory_path()};
static const fs::path mountDir0{mountRoot / "Slot_0"};
static const fs::path mountDir1{mountRoot / "Slot_1"};
static const fs::path mountDir2{mountRoot / "Slot_2"};
static const fs::path mountDir3{mountRoot / "Slot_3"};

static const fs::path remoteDir{"//192.168.10.101:445/images"};

} // namespace directories
