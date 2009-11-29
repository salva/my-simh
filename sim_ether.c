/* sim_ether.c: OS-dependent network routines
  ------------------------------------------------------------------------------
   Copyright (c) 2002-2007, David T. Hittner

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

  ------------------------------------------------------------------------------

  This ethernet simulation is based on the PCAP and WinPcap packages.

  PCAP/WinPcap was chosen as the basis for network code since it is the most
  "universal" of the various network packages available. Using this style has
  allowed rapid network development for the major SIMH platforms. Developing
  a network package specifically for SIMH was rejected due to the time required;
  the advantage would be a more easily compiled and integrated code set.

  There are various problems associated with use of ethernet networking, which
  would be true regardless of the network package used, since there are no
  universally accepted networking methods. The most serious of these is getting
  the proper networking package loaded onto the system, since most environments
  do not come with the network interface packages loaded.

  The second most serious network issue relates to security. The network
  simulation needs to simulate operating system level functionality (packet
  driving). However, the host network programming interfaces tend to operate at
  the user level of functionality, so getting to the full functionality of
  the network interface usually requires that the person executing the
  network code be a privileged user of the host system. See the PCAP/WinPcap
  documentation for the appropriate host platform if unprivileged use of
  networking is needed - there may be known workarounds.

  Define one of the two macros below to enable networking:
    USE_NETWORK     - Create statically linked network code
    USE_SHARED      - Create dynamically linked network code (_WIN32 only)

  ------------------------------------------------------------------------------

  Supported/Tested Platforms:

  Windows(NT,2K,XP,2K3)     WinPcap         V3.0+
  Linux                     libpcap at least 0.9
  OpenBSD,FreeBSD,NetBSD    libpcap at least 0.9
  MAC OS/X                  libpcap at least 0.9
  Solaris Sparc             libpcap at least 0.9
  Solaris Intel             libpcap at least 0.9
  AIX                       ??
  HP/UX                     ??
  Compaq Tru64 Unix         ??
  VMS                       Alpha/Itanium VMS only, needs VMS libpcap
  
  WinPcap is available from: 
                        http://winpcap.polito.it/
  libpcap for VMS is available from: 
                        http://simh.trailing-edge.com/sources/vms-pcap.zip
  libpcap for other Unix platforms is available at: 
        Current Version:  http://www.tcpdump.org/daily/libpcap-current.tar.gz
        Released Version: http://www.tcpdump.org/release/
        Note: You can only use the released version if it is at least 
              version 0.9

        
        We've gotten the tarball, unpacked, built and installed it with:
            gzip -dc libpcap-current.tar.gz | tar xvf -
            cd libpcap-directory-name
            ./configure
            make
            make install
        Note:  The "make install" step generally will have to be done as root.
        This will install libpcap in /usr/local/lib and /usr/local/include
        It is then important to make sure that you get the just installed 
        libpcap components referenced during your build.  This is generally 
        achieved by invoking gcc with: 
             -isystem /usr/local/include -L /usr/local/lib


  Note: Building for the platforms indicated above, with the indicated libpcap, 
  should automatically leverage the appropriate mechanisms contained here.  
  Things are structured so that it is likely to work for any other as yet 
  untested platform.  If it works for you, please let the author know so we 
  can update the table above.  If it doesn't work, then the following #define 
  variables can influence the operation on an untested platform.

  USE_BPF           - Determines if this code leverages a libpcap/WinPcap 
                      provided bpf packet filtering facility.  All tested 
                      environments have bpf facilities that work the way we 
                      need them to.  However a new one might not.  undefine 
                      this variable to let this code do its own filtering.
  USE_SETNONBLOCK   - Specifies whether the libpcap environment's non-blocking 
                      semantics are to be leveraged.  This helps to manage the 
                      varying behaviours of the kernel packet facilities 
                      leveraged by libpcap.
  USE_READER_THREAD - Specifies that packet reading should be done in the 
                      context of a separate thread.  The Posix threading 
                      APIs are used.  This option is less efficient than the
                      default non-threaded approach, but it exists since some 
                      platforms don't want to work with nonblocking libpcap 
                      semantics.   OpenBSD and NetBSD either don't have pthread 
                      APIs available, or they are too buggy to be useful. 
                      Using the threaded approach may require special compile 
                      and/or link time switches (i.e. -lpthread or -pthread, 
                      etc.) Consult the documentation for your platform as 
                      needed.
  MUST_DO_SELECT    - Specifies that when USE_READER_THREAD is active, that 
                      select() should be used to determin when available 
                      packets are ready for reading.  Otherwise, we depend 
                      on the libpcap/kernel packet timeout specified on 
                      pcap_open_live.  If USE_READER_THREAD is not set, then 
                      MUST_DO_SELECT is irrelevant

  NEED_PCAP_SENDPACKET
                    - Specifies that you are using an older version of libpcap
                      which doesn't provide a pcap_sendpacket API.

  NOTE: Changing these defines is done in either sim_ether.h OR on the global 
        compiler command line which builds all of the modules included in a
        simulator.

  ------------------------------------------------------------------------------

  Modification history:

  17-May-07  DTH  Fixed non-ethernet device removal loop (from Naoki Hamada)
  15-May-07  DTH  Added dynamic loading of wpcap.dll;
                  Corrected exceed max index bug in ethX lookup
  04-May-07  DTH  Corrected failure to look up ethernet device names in
                  the registry on Windows XP x64
  10-Jul-06  RMS  Fixed linux conditionalization (from Chaskiel Grundman)
  02-Jun-06  JDB  Fixed compiler warning for incompatible sscanf parameter
  15-Dec-05  DTH  Patched eth_host_devices [remove non-ethernet devices]
                  (from Mark Pizzolato and Galen Tackett, 08-Jun-05)
                  Patched eth_open [tun fix](from Antal Ritter, 06-Oct-05)
  30-Nov-05  DTH  Added option to regenerate CRC on received packets; some
                  ethernet devices need to pass it on to the simulation, and by
                  the time libpcap/winpcap gets the packet, the host OS network
                  layer has already stripped CRC out of the packet
  01-Dec-04  DTH  Added Windows user-defined adapter names (from Timothe Litt)
  25-Mar-04  MP   Revised comments and minor #defines to deal with updated
                  libpcap which now provides pcap_sendpacket on all platforms.
  04-Feb-04  MP   Returned success/fail status from eth_write to support
                  determining if the current libpcap connection can successfully 
                  write packets.
                  Added threaded approach to reading packets since
                  this works better on some platforms (solaris intel) than the 
                  inconsistently implemented non-blocking read approach.
  04-Feb-04  DTH  Converted ETH_DEBUG to sim_debug
  13-Jan-04  MP   tested and fixed on OpenBSD, NetBS and FreeBSD.
  09-Jan-04  MP   removed the BIOCSHDRCMPLT ioctl() for OS/X
  05-Jan-04  DTH  Added eth_mac_scan
  30-Dec-03  DTH  Cleaned up queue routines, added no network support message
  26-Dec-03  DTH  Added ethernet show and queue functions from pdp11_xq
  15-Dec-03  MP   polished generic libpcap support.
  05-Dec-03  DTH  Genericized eth_devices() and #ifdefs
  03-Dec-03  MP   Added Solaris support
  02-Dec-03  DTH  Corrected decnet fix to use reflection counting
  01-Dec-03  DTH  Added BPF source filtering and reflection counting
  28-Nov-03  DTH  Rewrote eth_devices using universal pcap_findalldevs()
  25-Nov-03  DTH  Verified DECNET_FIX, reversed ifdef to mainstream code
  19-Nov-03  MP   Fixed BPF functionality on Linux/BSD.
  17-Nov-03  DTH  Added xBSD simplification
  14-Nov-03  DTH  Added #ifdef DECNET_FIX for problematic duplicate detection code
  13-Nov-03  DTH  Merged in __FreeBSD__ support
  21-Oct-03  MP   Added enriched packet dumping for debugging
  20-Oct-03  MP   Added support for multiple ethernet devices on VMS
  20-Sep-03  Ankan Add VMS support (Alpha only)
  29-Sep-03  MP   Changed separator character in eth_fmt_mac to be ":" to
                  format ethernet addresses the way the BPF compile engine
                  wants to see them.
                  Added BPF support to filter packets
                  Added missing printf in eth_close
  07-Jun-03  MP   Added WIN32 support for DECNET duplicate address detection.
  06-Jun-03  MP   Fixed formatting of Ethernet Protocol Type in eth_packet_trace
  30-May-03  DTH  Changed WIN32 to _WIN32 for consistency
  07-Mar-03  MP   Fixed Linux implementation of PacketGetAdapterNames to also
                  work on Red Hat 6.2-sparc and Debian 3.0r1-sparc.
  03-Mar-03  MP   Changed logging to be consistent on stdout and sim_log
  01-Feb-03  MP   Changed type of local variables in eth_packet_trace to
                  conform to the interface needs of eth_mac_fmt wich produces
                  char data instead of unsigned char data.  Suggested by the
                  DECC compiler.
  15-Jan-03  DTH  Corrected PacketGetAdapterNames parameter2 datatype
  26-Dec-02  DTH  Merged Mark Pizzolato's enhancements with main source
                  Added networking documentation
                  Changed _DEBUG to ETH_DEBUG
  20-Dec-02  MP   Added display of packet CRC to the eth_packet_trace.
                  This helps distinguish packets with identical lengths
                  and protocols.
  05-Dec-02  MP   With the goal of draining the input buffer more rapidly
                  changed eth_read to call pcap_dispatch repeatedly until
                  either a timeout returns nothing or a packet allowed by
                  the filter is seen.  This more closely reflects how the
                  pcap layer will work when the filtering is actually done
                  by a bpf filter.
  31-Oct-02  DTH  Added USE_NETWORK conditional
                  Reworked not attached test
                  Added OpenBSD support (from Federico Schwindt)
                  Added ethX detection simplification (from Megan Gentry)
                  Removed sections of temporary code
                  Added parameter validation
  23-Oct-02  DTH  Beta 5 released
  22-Oct-02  DTH  Added all_multicast and promiscuous support
                  Fixed not attached behavior
  21-Oct-02  DTH  Added NetBSD support (from Jason Thorpe)
                  Patched buffer size to make sure entire packet is read in
                  Made 'ethX' check characters passed as well as length
                  Corrected copyright again
  16-Oct-02  DTH  Beta 4 released
                  Corrected copyright
  09-Oct-02  DTH  Beta 3 released
                  Added pdp11 write acceleration (from Patrick Caulfield)
  08-Oct-02  DTH  Beta 2 released
                  Integrated with 2.10-0p4
                  Added variable vector and copyrights
  04-Oct-02  DTH  Added linux support (from Patrick Caulfield)
  03-Oct-02  DTH  Beta version of xq/sim_ether released for SIMH 2.09-11
  24-Sep-02  DTH  Finished eth_devices, eth_getname
  18-Sep-02  DTH  Callbacks implemented
  13-Sep-02  DTH  Basic packet read/write written
  20-Aug-02  DTH  Created Sim_Ether for O/S independant ethernet implementation

  ------------------------------------------------------------------------------
*/

#include <ctype.h>
#include "sim_ether.h"
#include "sim_sock.h"

extern FILE *sim_log;


/*============================================================================*/
/*                  OS-independant ethernet routines                          */
/*============================================================================*/

t_stat eth_mac_scan (ETH_MAC* mac, char* strmac)
{
  int i, j;
  short unsigned int num;
  char cptr[18];
  int len = strlen(strmac);
  const ETH_MAC zeros = {0,0,0,0,0,0};
  const ETH_MAC ones  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  ETH_MAC newmac;

  /* format of string must be 6 double-digit hex bytes with valid separators
     ideally, this mac scanner could allow more flexible formatting later */
  if (len != 17) return SCPE_ARG;

  /* copy string to local storage for mangling */
  strcpy(cptr, strmac);

  /* make sure byte separators are OK */
  for (i=2; i<len; i=i+3) {
    if ((cptr[i] != '-') && 
        (cptr[i] != '.') &&
        (cptr[i] != ':')) return SCPE_ARG;
    cptr[i] = '\0';
  }

  /* get and set address bytes */
  for (i=0, j=0; i<len; i=i+3, j++) {
    int valid = strspn(&cptr[i], "0123456789abcdefABCDEF");
    if (valid < 2) return SCPE_ARG;
    sscanf(&cptr[i], "%hx", &num);
    newmac[j] = (unsigned char) num;
  }

  /* final check - mac cannot be broadcast or multicast address */
  if (!memcmp(newmac, zeros, sizeof(ETH_MAC)) ||  /* broadcast */
      !memcmp(newmac, ones,  sizeof(ETH_MAC)) ||  /* broadcast */
      (newmac[0] & 0x01)                          /* multicast */
     )
    return SCPE_ARG;

  /* new mac is OK, copy into passed mac */
  memcpy (*mac, newmac, sizeof(ETH_MAC));
  return SCPE_OK;
}

void eth_mac_fmt(ETH_MAC* mac, char* buff)
{
  uint8* m = (uint8*) mac;
  sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return;
}

static const uint32 crcTable[256] = {
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
  0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
  0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
  0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
  0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
  0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
  0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
  0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
  0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
  0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
  0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
  0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
  0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
  0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
  0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
  0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
  0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
  0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
  0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
  0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32 eth_crc32(uint32 crc, const void* vbuf, size_t len)
{
  const uint32 mask = 0xFFFFFFFF;
  const unsigned char* buf = (const unsigned char*)vbuf;

  crc ^= mask;
  while (0 != len--)
    crc = (crc >> 8) ^ crcTable[ (crc ^ (*buf++)) & 0xFF ];
  return(crc ^ mask);
}

void eth_add_crc32(ETH_PACK* packet)
{
  if (packet->len <= ETH_MAX_PACKET) {
    uint32 crc = eth_crc32(0, packet->msg, packet->len);  /* calculate CRC */
    uint32 ncrc = htonl(crc);                             /* CRC in network order */
    int size = sizeof(ncrc);                              /* size of crc field */
    memcpy(&packet->msg[packet->len], &ncrc, size);       /* append crc to packet */
    packet->crc_len = packet->len + size;                 /* set packet crc length */
  } else {
    packet->crc_len = 0;                                  /* appending crc would destroy packet */
  }
}

void eth_setcrc(ETH_DEV* dev, int need_crc)
{
  dev->need_crc = need_crc;
}

void eth_packet_trace_ex(ETH_DEV* dev, const uint8 *msg, int len, char* txt, int dmp)
{
  if (dev->dptr->dctrl & dev->dbit) {
    char src[20];
    char dst[20];
    unsigned short* proto = (unsigned short*) &msg[12];
    uint32 crc = eth_crc32(0, msg, len);
    eth_mac_fmt((ETH_MAC*)&msg[0], dst);
    eth_mac_fmt((ETH_MAC*)&msg[6], src);
    sim_debug(dev->dbit, dev->dptr, "%s  dst: %s  src: %s  proto: 0x%04X  len: %d  crc: %X\n",
          txt, dst, src, ntohs(*proto), len, crc);
    if (dmp) {
      int i, same, group, sidx, oidx;
      char outbuf[80], strbuf[18];
      static char hex[] = "0123456789ABCDEF";
      for (i=same=0; i<len; i += 16) {
        if ((i > 0) && (0 == memcmp(&msg[i], &msg[i-16], 16))) {
          ++same;
          continue;
        }
        if (same > 0) {
          sim_debug(dev->dbit, dev->dptr, "%04X thru %04X same as above\r\n", i-(16*same), i-1);
          same = 0;
        }
        group = (((len - i) > 16) ? 16 : (len - i));
        for (sidx=oidx=0; sidx<group; ++sidx) {
          outbuf[oidx++] = ' ';
          outbuf[oidx++] = hex[(msg[i+sidx]>>4)&0xf];
          outbuf[oidx++] = hex[msg[i+sidx]&0xf];
          if (isprint(msg[i+sidx]))
            strbuf[sidx] = msg[i+sidx];
          else
            strbuf[sidx] = '.';
        }
        outbuf[oidx] = '\0';
        strbuf[sidx] = '\0';
        sim_debug(dev->dbit, dev->dptr, "%04X%-48s %s\r\n", i, outbuf, strbuf);
      }
      if (same > 0)
        sim_debug(dev->dbit, dev->dptr, "%04X thru %04X same as above\r\n", i-(16*same), len-1);
    }
  }
}

void eth_packet_trace(ETH_DEV* dev, const uint8 *msg, int len, char* txt)
{
  eth_packet_trace_ex(dev, msg, len, txt, 1/*len > ETH_MAX_PACKET*/);
}

/* strncasecmp() is not available on all platforms */
int eth_strncasecmp(char* string1, char* string2, int len)
{
  int i;
  unsigned char s1, s2;

  for (i=0; i<len; i++) {
    s1 = string1[i];
    s2 = string2[i];
    if (islower (s1)) s1 = toupper (s1);
    if (islower (s2)) s2 = toupper (s2);

    if (s1 < s2)
      return -1;
    if (s1 > s2)
      return 1;
    if (s1 == 0) return 0;
  }
  return 0;
}

void eth_zero(ETH_DEV* dev)
{
  /* set all members to NULL OR 0 */
  memset(dev, 0, sizeof(ETH_DEV));
  dev->reflections = -1;                          /* not established yet */
}

t_stat eth_show (FILE* st, UNIT* uptr, int32 val, void* desc)
{
  ETH_LIST  list[ETH_MAX_DEVICE];
  int number = 0;

  fprintf(st, "ETH devices:\n");
  if (number == -1)
    fprintf(st, "  network support not available in simulator\n");
  else
    if (number == 0)
      fprintf(st, "  no network devices are available\n");
    else {
      int i, min, len;
      for (i=0, min=0; i<number; i++)
        if ((len = strlen(list[i].name)) > min) min = len;
      for (i=0; i<number; i++)
        fprintf(st,"  %d  %-*s (%s)\n", i, min, list[i].name, list[i].desc);
    }
  return SCPE_OK;
}

t_stat ethq_init(ETH_QUE* que, int max)
{
  /* create dynamic queue if it does not exist */
  if (!que->item) {
    size_t size = sizeof(struct eth_item) * max;
    que->max = max;
    que->item = (struct eth_item *) malloc(size);
    if (que->item) {
      /* init dynamic memory */
      memset(que->item, 0, size);
    } else {
      /* failed to allocate memory */
      char* msg = "EthQ: failed to allocate dynamic queue[%d]\r\n";
      printf(msg, max);
      if (sim_log) fprintf(sim_log, msg, max);
      return SCPE_MEM;
    };
  };
  return SCPE_OK;
}

void ethq_clear(ETH_QUE* que)
{
  /* clear packet array */
  memset(que->item, 0, sizeof(struct eth_item) * que->max);
  /* clear rest of structure */
  que->count = que->head = que->tail = que->loss = que->high = 0;
}

void ethq_remove(ETH_QUE* que)
{
  struct eth_item* item = &que->item[que->head];

  if (que->count) {
    memset(item, 0, sizeof(struct eth_item));
    if (++que->head == que->max)
      que->head = 0;
    que->count--;
  }
}

void ethq_insert(ETH_QUE* que, int32 type, ETH_PACK* pack, int32 status)
{
  struct eth_item* item;

  /* if queue empty, set pointers to beginning */
  if (!que->count) {
    que->head = 0;
    que->tail = -1;
  }

  /* find new tail of the circular queue */
  if (++que->tail == que->max)
    que->tail = 0;
  if (++que->count > que->max) {
    que->count = que->max;
    /* lose oldest packet */
    if (++que->head == que->max)
      que->head = 0;
    que->loss++;
    }
  if (que->count > que->high)
    que->high = que->count;

  /* set information in (new) tail item */
  item = &que->item[que->tail];
  item->type = type;
  item->packet.len = pack->len;
  item->packet.used = 0;
  item->packet.crc_len = pack->crc_len;
  memcpy(item->packet.msg, pack->msg, ((pack->len > pack->crc_len) ? pack->len : pack->crc_len));
  item->packet.status = status;
}

/*============================================================================*/
/*                        Non-implemented versions                            */
/*============================================================================*/

#if !defined (USE_NETWORK) && !defined(USE_SHARED)
t_stat eth_open(ETH_DEV* dev, char* name, DEVICE* dptr, uint32 dbit)
  {return SCPE_NOFNC;}
t_stat eth_close (ETH_DEV* dev)
  {return SCPE_NOFNC;}
t_stat eth_write (ETH_DEV* dev, ETH_PACK* packet, ETH_PCALLBACK routine)
  {return SCPE_NOFNC;}
t_stat eth_read (ETH_DEV* dev, ETH_PACK* packet, ETH_PCALLBACK routine)
  {return SCPE_NOFNC;}
t_stat eth_filter (ETH_DEV* dev, int addr_count, ETH_MAC* addresses,
                   ETH_BOOL all_multicast, ETH_BOOL promiscuous)
  {return SCPE_NOFNC;}
int eth_devices (int max, ETH_LIST* dev)
  {return -1;}
#else       /* endif unimplemented */

/*============================================================================*/
/*      WIN32, Linux, and xBSD routines use WinPcap and libpcap packages      */
/*        OpenVMS Alpha uses a WinPcap port and an associated execlet         */
/*============================================================================*/

#if defined (xBSD) && !defined(__APPLE__)
#include <sys/ioctl.h>
#include <net/bpf.h>
#endif /* xBSD */

#include <libvdeplug.h>
#include <string.h>
#include <unistd.h>

/* Allows windows to look up user-defined adapter names */
#if defined(_WIN32)
#include <winreg.h>
#endif


t_stat eth_open(ETH_DEV* dev, char* name, DEVICE* dptr, uint32 dbit)
{
  const int bufsz = (BUFSIZ < ETH_MAX_PACKET) ? ETH_MAX_PACKET : BUFSIZ;
  char temp[1024];
  char* msg;
  struct vde_open_args args;

  /* initialize device */
  eth_zero(dev);
  memset(&args, 0, sizeof(args));
  args.mode = 0700;

  if (strncmp(name, "vde", 3) == 0 &&
      (name[3] == '\0' ||
       (name[3] == ':' && name[4] != '\0'))) {
    strncpy(temp, name, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';
    if (name[3] == '\0') {
      temp[3] = ':';
      strcpy(temp + 4, "vde");
    }
  } else {
    msg = "Eth: bad interface name - %s\r\n";
    printf (msg, name);
    if (sim_log) fprintf (sim_log, msg, name);
    return SCPE_OPENERR;
  }
  dev->handle = (void *) vde_open(temp + 4, "SimH", &args);
  if (dev->handle) {
    msg =  "Eth: connected to VDE %s\r\n";
    printf(msg, temp);
    if (sim_log) fprintf (sim_log, msg, temp);
  } else {  /* can't open device */
    msg = "Eth: unable to connect to VDE at %s: %s\r\n";
    printf(msg, temp, strerror(errno));
    if (sim_log) fprintf (sim_log, msg, temp, strerror(errno));
    return SCPE_OPENERR;
  }

  /* save name of device */
  dev->name = malloc(strlen(temp)+1);
  strcpy(dev->name, temp);

  /* save debugging information */
  dev->dptr = dptr;
  dev->dbit = dbit;

  return SCPE_OK;
}

t_stat eth_close(ETH_DEV* dev)
{
  char* msg = "Eth: closed %s\r\n";


  /* make sure device exists */
  if (!dev) return SCPE_UNATT;

  /* close the device */
  vde_close((VDECONN *)dev->handle);
  printf (msg, dev->name);
  if (sim_log) fprintf (sim_log, msg, dev->name);

  /* free memory */
  free(dev->name);
  eth_zero(dev);
  return SCPE_OK;
}

t_stat eth_reflect(ETH_DEV* dev, ETH_MAC mac)
{
  ETH_PACK send, recv;
  t_stat status;
  int i;
  struct timeval delay;

  /* build a packet */
  memset (&send, 0, sizeof(ETH_PACK));
  send.len = ETH_MIN_PACKET;                              /* minimum packet size */
  memcpy(&send.msg[0], mac, sizeof(ETH_MAC));             /* target address */
  memcpy(&send.msg[6], mac, sizeof(ETH_MAC));             /* source address */
  send.msg[12] = 0x90;                                    /* loopback packet type */
  for (i=14; i<send.len; i++)
    send.msg[i] = 32 + i;                                 /* gibberish */

  dev->reflections = 0;
  eth_filter(dev, 1, (ETH_MAC *)mac, 0, 0);

  /* send the packet */
  status = eth_write (dev, &send, NULL);
  if (status != SCPE_OK) {
    char *msg;
    msg = "Eth: Error Transmitting packet: %s\r\n"
          "You may need to run as root, or install a libpcap version\r\n"
          "which is at least 0.9 from www.tcpdump.org\r\n";
    printf(msg, strerror(errno));
    if (sim_log) fprintf (sim_log, msg, strerror(errno));
    return status;
  }

  /* if/when we have a sim_os_msleep() we'll use it here instead of this select() */
  delay.tv_sec = 0;
  delay.tv_usec = 50*1000;
  select(0, NULL, NULL, NULL, &delay); /* make sure things settle into the read path */

  /* empty the read queue and count the reflections */
  do {
    memset (&recv, 0, sizeof(ETH_PACK));
    status = eth_read (dev, &recv, NULL);
    if (memcmp(send.msg, recv.msg, ETH_MIN_PACKET)== 0)
      dev->reflections++;
  } while (recv.len > 0);

  sim_debug(dev->dbit, dev->dptr, "Reflections = %d\n", dev->reflections);
  return dev->reflections;
}

t_stat eth_write(ETH_DEV* dev, ETH_PACK* packet, ETH_PCALLBACK routine)
{
  int error = 1;   /* default to failure */

  /* make sure device exists */
  if (!dev) return SCPE_UNATT;

  /* make sure packet exists */
  if (!packet) return SCPE_ARG;

  /* make sure packet is acceptable length */
  if ((packet->len >= ETH_MIN_PACKET) && (packet->len <= ETH_MAX_PACKET)) {
    eth_packet_trace (dev, packet->msg, packet->len, "writing");

    /* dispatch write request (synchronous; no need to save write info to dev) */
    error = (vde_send((VDECONN*)dev->handle, (u_char*)packet->msg, packet->len, 0) != packet->len);

    /* detect sending of decnet loopback packet */
    if ((error == 0) && DECNET_SELF_FRAME(dev->decnet_addr, packet->msg)) 
      dev->decnet_self_sent += dev->reflections;

  } /* if packet->len */

  /* call optional write callback function */
  if (routine) (routine)(error);

  return ((error == 0) ? SCPE_OK : SCPE_IOERR);
}

void eth_packet_read(ETH_DEV *dev, const u_char* data, ssize_t len)
{
  int to_me = 0;
  int from_me = 0;
  int i;

  for (i = 0; i < dev->addr_count; i++) {
    if (memcmp(data, dev->filter_address[i], 6) == 0) to_me = 1;
    if (memcmp(data + 6, dev->filter_address[i], 6) == 0) from_me = 1;
  }

  if (dev->all_multicast && (data[0] & 0x01)) to_me = 1;
  if (dev->promiscuous) to_me = 1;

  /* detect sending of decnet loopback packet */
  if (DECNET_SELF_FRAME(dev->decnet_addr, data)) {
    /* lower reflection count - if already zero, pass it on */
    if (dev->decnet_self_sent > 0) {
      dev->decnet_self_sent--;
      to_me = 0;
    } 
  }
  else
    from_me = 0;

  if (to_me && !from_me) {
    /* set data in passed read packet */
    dev->read_packet->len = len;
    memcpy(dev->read_packet->msg, data, len);
    if (dev->need_crc)
      eth_add_crc32(dev->read_packet);

    eth_packet_trace (dev, dev->read_packet->msg, dev->read_packet->len, "reading");

    /* call optional read callback function */
    if (dev->read_callback)
      (dev->read_callback)(0);
  }
}

t_stat eth_read(ETH_DEV* dev, ETH_PACK* packet, ETH_PCALLBACK routine)
{
  int status;
  ssize_t len;
  VDECONN *vdeconn = (VDECONN *)dev->handle;
  int vdefd = vde_datafd(vdeconn);
  fd_set vdeset;
  struct timeval timeout;
  char buffer[2048];

  /* make sure device exists */
  if (!dev) return SCPE_UNATT;

  /* make sure packet exists */
  if (!packet) return SCPE_ARG;

  /* set read packet */
  dev->read_packet = packet;
  packet->len = 0;

  /* set optional callback routine */
  dev->read_callback = routine;

  /* dispatch read request to either receive a filtered packet or timeout */
  while (1) {
    FD_ZERO(&vdeset);
    FD_SET(vdefd, &vdeset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (select(vdefd + 1, &vdeset, NULL, NULL, &timeout) > 0) {
      len = vde_recv(vdeconn, buffer, sizeof(buffer), 0);
      if (len > 0) {
	// printf("!");
	eth_packet_read(dev, buffer, len);
      }
    }
    else
      break;
  }
  return SCPE_OK;
}

t_stat eth_filter(ETH_DEV* dev, int addr_count, ETH_MAC* addresses,
                  ETH_BOOL all_multicast, ETH_BOOL promiscuous)
{
  int i;
  char buf[110+66*ETH_FILTER_MAX];
  char mac[20];
  char* buf2;
  t_stat status;

  /* make sure device exists */
  if (!dev) return SCPE_UNATT;

  /* filter count OK? */
  if ((addr_count < 0) || (addr_count > ETH_FILTER_MAX))
    return SCPE_ARG;
  else
    if (!addresses) return SCPE_ARG;

  /* set new filter addresses */
  for (i = 0; i < addr_count; i++)
    memcpy(dev->filter_address[i], addresses[i], sizeof(ETH_MAC));
  dev->addr_count = addr_count;

  /* store other flags */
  dev->all_multicast = all_multicast;
  dev->promiscuous   = promiscuous;

  /* print out filter information if debugging */
  if (dev->dptr->dctrl & dev->dbit) {
    sim_debug(dev->dbit, dev->dptr, "Filter Set\n");
    for (i = 0; i < addr_count; i++) {
      char mac[20];
      eth_mac_fmt(&dev->filter_address[i], mac);
      sim_debug(dev->dbit, dev->dptr, "  Addr[%d]: %s\n", i, mac);
    }
    if (dev->all_multicast)
      sim_debug(dev->dbit, dev->dptr, "All Multicast\n");
    if (dev->promiscuous)
      sim_debug(dev->dbit, dev->dptr, "Promiscuous\n");
  }

  /* test reflections */
  if (dev->reflections == -1)
    status = eth_reflect(dev, dev->filter_address[0]);

  /* setup BPF filters and other fields to minimize packet delivery */
  strcpy(buf, "");

  /* construct destination filters - since the real ethernet interface was set
     into promiscuous mode by eth_open(), we need to filter out the packets that
     our simulated interface doesn't want. */
  if (!dev->promiscuous) {
    for (i = 0; i < addr_count; i++) {
      eth_mac_fmt(&dev->filter_address[i], mac);
      if (!strstr(buf, mac))    /* eliminate duplicates */
        sprintf(&buf[strlen(buf)], "%s(ether dst %s)", (*buf) ? " or " : "", mac);
    }
    if (dev->all_multicast)
      sprintf(&buf[strlen(buf)], "%s(ether multicast)", (*buf) ? " or " : "");
  }

  /* construct source filters - this prevents packets from being reflected back 
     by systems where WinPcap and libpcap cause packet reflections. Note that
     some systems do not reflect packets at all. This *assumes* that the 
     simulated NIC will not send out packets with multicast source fields. */
  if ((addr_count > 0) && (dev->reflections > 0)) {
    if (strlen(buf) > 0)
      sprintf(&buf[strlen(buf)], " and ");
    sprintf (&buf[strlen(buf)], "not (");
    buf2 = &buf[strlen(buf)];
    for (i = 0; i < addr_count; i++) {
      if (dev->filter_address[i][0] & 0x01) continue; /* skip multicast addresses */
      eth_mac_fmt(&dev->filter_address[i], mac);
      if (!strstr(buf2, mac))   /* eliminate duplicates */
        sprintf(&buf2[strlen(buf2)], "%s(ether src %s)", (*buf2) ? " or " : "", mac);
    }
    sprintf (&buf[strlen(buf)], ")");
  }
  /* When starting, DECnet sends out a packet with the source and destination
     addresses set to the same value as the DECnet MAC address. This packet is
     designed to find and help diagnose DECnet address conflicts. Normally, this
     packet would not be seen by the sender, only by the other machine that has
     the same DECnet address. If the ethernet subsystem is reflecting packets,
     DECnet will fail to start if it sees the reflected packet, since it thinks
     another system is using this DECnet address. We have to let these packets
     through, so that if another machine has the same DECnet address that we
     can detect it. Both eth_write() and eth_callback() help by checking the
     reflection count - eth_write() adds the reflection count to
     dev->decnet_self_sent, and eth_callback() check the value - if the
     dev->decnet_self_sent count is zero, then the packet has come from another
     machine with the same address, and needs to be passed on to the simulated
     machine. */
  memset(dev->decnet_addr, 0, sizeof(ETH_MAC));
  /* check for decnet address in filters */
  if ((addr_count) && (dev->reflections > 0)) {
    for (i = 0; i < addr_count; i++) {
      eth_mac_fmt(&dev->filter_address[i], mac);
      if (memcmp(mac, "AA:00:04", 8) == 0) {
        memcpy(dev->decnet_addr, &dev->filter_address[i], sizeof(ETH_MAC));
        /* let packets through where dst and src are the same as our decnet address */
        sprintf (&buf[strlen(buf)], " or ((ether dst %s) and (ether src %s))", mac, mac);
        break;
      }
    }
  }
  sim_debug(dev->dbit, dev->dptr, "BPF string is: |%s|\n", buf);

  return SCPE_OK;
}

#endif /* USE_NETWORK */
