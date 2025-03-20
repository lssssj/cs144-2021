#include "tun.hh"

#include "file_descriptor.hh"
#include "util.hh"
#ifdef __APPLE__
#include <sys/socket.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <net/if_utun.h>
#include <string>
#include <fcntl.h>
#include <sys/ioctl.h>

TunTapFD::TunTapFD(const std::string &devname, const bool is_tun)
    : FileDescriptor(SystemCall("socket", socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL))) {
    // 1. 创建控制套接字
   
   (void)is_tun;

    // 2. 绑定到 utun 设备
    struct ctl_info ctl_info = {};
    strncpy(ctl_info.ctl_name, UTUN_CONTROL_NAME, sizeof(ctl_info.ctl_name));
    if (ioctl(fd_num(), CTLIOCGINFO, &ctl_info) == -1) {
        close();
        throw std::system_error(errno, std::system_category(), "ioctl(CTLIOCGINFO) failed");
    }

    struct sockaddr_ctl sc = {};
    sc.sc_id = ctl_info.ctl_id;
    sc.sc_len = sizeof(sc);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    sc.sc_unit = 0;  // 内核自动分配 utunX 编号

    if (connect(fd_num(), reinterpret_cast<struct sockaddr*>(&sc), sizeof(sc)) == -1) {
        close();
        throw std::system_error(errno, std::system_category(), "connect(AF_SYS_CONTROL) failed");
    }

    // 3. 设置接口名称（如 utun0）
    uint32_t opt = 0;
    if (devname.find("utun") == 0) {
        opt = atoi(devname.substr(4).c_str()) + 1;
    }
    if (setsockopt(fd_num(), SYSPROTO_CONTROL, UTUN_OPT_IFNAME, &opt, sizeof(opt)) == -1) {
        close();
        throw std::system_error(errno, std::system_category(), "setsockopt(UTUN_OPT_IFNAME) failed");
    }

    // 4. 存储文件描述符
}
#else
#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>

static constexpr const char *CLONEDEV = "/dev/net/tun";

using namespace std;

//! \param[in] devname is the name of the TUN or TAP device, specified at its creation.
//! \param[in] is_tun is `true` for a TUN device (expects IP datagrams), or `false` for a TAP device (expects Ethernet frames)
//!
//! To create a TUN device, you should already have run
//!
//!     ip tuntap add mode tun user `username` name `devname`
//!
//! as root before calling this function.

TunTapFD::TunTapFD(const string &devname, const bool is_tun)
    : FileDescriptor(SystemCall("open", open(CLONEDEV, O_RDWR))) {
    struct ifreq tun_req {};

    tun_req.ifr_flags = (is_tun ? IFF_TUN : IFF_TAP) | IFF_NO_PI;  // tun device with no packetinfo

    // copy devname to ifr_name, making sure to null terminate

    strncpy(static_cast<char *>(tun_req.ifr_name), devname.data(), IFNAMSIZ - 1);
    tun_req.ifr_name[IFNAMSIZ - 1] = '\0';

    SystemCall("ioctl", ioctl(fd_num(), TUNSETIFF, static_cast<void *>(&tun_req)));
}
#endif