/*
 * Copyright (c) 2003, 2025, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014, 2025, Red Hat Inc. All rights reserved.
 * Copyright (c) 2020, 2022, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/stubRoutines.hpp"
#include "utilities/globalDefinitions.hpp"

// Implementation of the platform-specific part of StubRoutines - for
// a description of how to extend it, see the stubRoutines.hpp file.


// define fields for arch-specific entries

#define DEFINE_ARCH_ENTRY(arch, blob_name, stub_name, field_name, getter_name) \
  address StubRoutines:: arch :: STUB_FIELD_NAME(field_name)  = nullptr;

#define DEFINE_ARCH_ENTRY_INIT(arch, blob_name, stub_name, field_name, getter_name, init_function) \
  address StubRoutines:: arch :: STUB_FIELD_NAME(field_name)  = CAST_FROM_FN_PTR(address, init_function);

STUBGEN_ARCH_ENTRIES_DO(DEFINE_ARCH_ENTRY, DEFINE_ARCH_ENTRY_INIT)

#undef DEFINE_ARCH_ENTRY_INIT
#undef DEFINE_ARCH_ENTRY

bool StubRoutines::riscv::_completed = false;

/**
 *  crc_table[] from jdk/src/java.base/share/native/libzip/zlib/crc32.h
 */

address StubRoutines::crc_table_addr()    { return (address)StubRoutines::riscv::_crc_table; }
address StubRoutines::crc32c_table_addr() { ShouldNotCallThis(); return nullptr; }

ATTRIBUTE_ALIGNED(4096) juint StubRoutines::riscv::_crc_table[] =
{
    // Table 0
    0x00000000UL, 0x77073096UL, 0xee0e612cUL, 0x990951baUL, 0x076dc419UL,
    0x706af48fUL, 0xe963a535UL, 0x9e6495a3UL, 0x0edb8832UL, 0x79dcb8a4UL,
    0xe0d5e91eUL, 0x97d2d988UL, 0x09b64c2bUL, 0x7eb17cbdUL, 0xe7b82d07UL,
    0x90bf1d91UL, 0x1db71064UL, 0x6ab020f2UL, 0xf3b97148UL, 0x84be41deUL,
    0x1adad47dUL, 0x6ddde4ebUL, 0xf4d4b551UL, 0x83d385c7UL, 0x136c9856UL,
    0x646ba8c0UL, 0xfd62f97aUL, 0x8a65c9ecUL, 0x14015c4fUL, 0x63066cd9UL,
    0xfa0f3d63UL, 0x8d080df5UL, 0x3b6e20c8UL, 0x4c69105eUL, 0xd56041e4UL,
    0xa2677172UL, 0x3c03e4d1UL, 0x4b04d447UL, 0xd20d85fdUL, 0xa50ab56bUL,
    0x35b5a8faUL, 0x42b2986cUL, 0xdbbbc9d6UL, 0xacbcf940UL, 0x32d86ce3UL,
    0x45df5c75UL, 0xdcd60dcfUL, 0xabd13d59UL, 0x26d930acUL, 0x51de003aUL,
    0xc8d75180UL, 0xbfd06116UL, 0x21b4f4b5UL, 0x56b3c423UL, 0xcfba9599UL,
    0xb8bda50fUL, 0x2802b89eUL, 0x5f058808UL, 0xc60cd9b2UL, 0xb10be924UL,
    0x2f6f7c87UL, 0x58684c11UL, 0xc1611dabUL, 0xb6662d3dUL, 0x76dc4190UL,
    0x01db7106UL, 0x98d220bcUL, 0xefd5102aUL, 0x71b18589UL, 0x06b6b51fUL,
    0x9fbfe4a5UL, 0xe8b8d433UL, 0x7807c9a2UL, 0x0f00f934UL, 0x9609a88eUL,
    0xe10e9818UL, 0x7f6a0dbbUL, 0x086d3d2dUL, 0x91646c97UL, 0xe6635c01UL,
    0x6b6b51f4UL, 0x1c6c6162UL, 0x856530d8UL, 0xf262004eUL, 0x6c0695edUL,
    0x1b01a57bUL, 0x8208f4c1UL, 0xf50fc457UL, 0x65b0d9c6UL, 0x12b7e950UL,
    0x8bbeb8eaUL, 0xfcb9887cUL, 0x62dd1ddfUL, 0x15da2d49UL, 0x8cd37cf3UL,
    0xfbd44c65UL, 0x4db26158UL, 0x3ab551ceUL, 0xa3bc0074UL, 0xd4bb30e2UL,
    0x4adfa541UL, 0x3dd895d7UL, 0xa4d1c46dUL, 0xd3d6f4fbUL, 0x4369e96aUL,
    0x346ed9fcUL, 0xad678846UL, 0xda60b8d0UL, 0x44042d73UL, 0x33031de5UL,
    0xaa0a4c5fUL, 0xdd0d7cc9UL, 0x5005713cUL, 0x270241aaUL, 0xbe0b1010UL,
    0xc90c2086UL, 0x5768b525UL, 0x206f85b3UL, 0xb966d409UL, 0xce61e49fUL,
    0x5edef90eUL, 0x29d9c998UL, 0xb0d09822UL, 0xc7d7a8b4UL, 0x59b33d17UL,
    0x2eb40d81UL, 0xb7bd5c3bUL, 0xc0ba6cadUL, 0xedb88320UL, 0x9abfb3b6UL,
    0x03b6e20cUL, 0x74b1d29aUL, 0xead54739UL, 0x9dd277afUL, 0x04db2615UL,
    0x73dc1683UL, 0xe3630b12UL, 0x94643b84UL, 0x0d6d6a3eUL, 0x7a6a5aa8UL,
    0xe40ecf0bUL, 0x9309ff9dUL, 0x0a00ae27UL, 0x7d079eb1UL, 0xf00f9344UL,
    0x8708a3d2UL, 0x1e01f268UL, 0x6906c2feUL, 0xf762575dUL, 0x806567cbUL,
    0x196c3671UL, 0x6e6b06e7UL, 0xfed41b76UL, 0x89d32be0UL, 0x10da7a5aUL,
    0x67dd4accUL, 0xf9b9df6fUL, 0x8ebeeff9UL, 0x17b7be43UL, 0x60b08ed5UL,
    0xd6d6a3e8UL, 0xa1d1937eUL, 0x38d8c2c4UL, 0x4fdff252UL, 0xd1bb67f1UL,
    0xa6bc5767UL, 0x3fb506ddUL, 0x48b2364bUL, 0xd80d2bdaUL, 0xaf0a1b4cUL,
    0x36034af6UL, 0x41047a60UL, 0xdf60efc3UL, 0xa867df55UL, 0x316e8eefUL,
    0x4669be79UL, 0xcb61b38cUL, 0xbc66831aUL, 0x256fd2a0UL, 0x5268e236UL,
    0xcc0c7795UL, 0xbb0b4703UL, 0x220216b9UL, 0x5505262fUL, 0xc5ba3bbeUL,
    0xb2bd0b28UL, 0x2bb45a92UL, 0x5cb36a04UL, 0xc2d7ffa7UL, 0xb5d0cf31UL,
    0x2cd99e8bUL, 0x5bdeae1dUL, 0x9b64c2b0UL, 0xec63f226UL, 0x756aa39cUL,
    0x026d930aUL, 0x9c0906a9UL, 0xeb0e363fUL, 0x72076785UL, 0x05005713UL,
    0x95bf4a82UL, 0xe2b87a14UL, 0x7bb12baeUL, 0x0cb61b38UL, 0x92d28e9bUL,
    0xe5d5be0dUL, 0x7cdcefb7UL, 0x0bdbdf21UL, 0x86d3d2d4UL, 0xf1d4e242UL,
    0x68ddb3f8UL, 0x1fda836eUL, 0x81be16cdUL, 0xf6b9265bUL, 0x6fb077e1UL,
    0x18b74777UL, 0x88085ae6UL, 0xff0f6a70UL, 0x66063bcaUL, 0x11010b5cUL,
    0x8f659effUL, 0xf862ae69UL, 0x616bffd3UL, 0x166ccf45UL, 0xa00ae278UL,
    0xd70dd2eeUL, 0x4e048354UL, 0x3903b3c2UL, 0xa7672661UL, 0xd06016f7UL,
    0x4969474dUL, 0x3e6e77dbUL, 0xaed16a4aUL, 0xd9d65adcUL, 0x40df0b66UL,
    0x37d83bf0UL, 0xa9bcae53UL, 0xdebb9ec5UL, 0x47b2cf7fUL, 0x30b5ffe9UL,
    0xbdbdf21cUL, 0xcabac28aUL, 0x53b39330UL, 0x24b4a3a6UL, 0xbad03605UL,
    0xcdd70693UL, 0x54de5729UL, 0x23d967bfUL, 0xb3667a2eUL, 0xc4614ab8UL,
    0x5d681b02UL, 0x2a6f2b94UL, 0xb40bbe37UL, 0xc30c8ea1UL, 0x5a05df1bUL,
    0x2d02ef8dUL,

    // Table 1
    0x00000000UL, 0x191b3141UL, 0x32366282UL, 0x2b2d53c3UL, 0x646cc504UL,
    0x7d77f445UL, 0x565aa786UL, 0x4f4196c7UL, 0xc8d98a08UL, 0xd1c2bb49UL,
    0xfaefe88aUL, 0xe3f4d9cbUL, 0xacb54f0cUL, 0xb5ae7e4dUL, 0x9e832d8eUL,
    0x87981ccfUL, 0x4ac21251UL, 0x53d92310UL, 0x78f470d3UL, 0x61ef4192UL,
    0x2eaed755UL, 0x37b5e614UL, 0x1c98b5d7UL, 0x05838496UL, 0x821b9859UL,
    0x9b00a918UL, 0xb02dfadbUL, 0xa936cb9aUL, 0xe6775d5dUL, 0xff6c6c1cUL,
    0xd4413fdfUL, 0xcd5a0e9eUL, 0x958424a2UL, 0x8c9f15e3UL, 0xa7b24620UL,
    0xbea97761UL, 0xf1e8e1a6UL, 0xe8f3d0e7UL, 0xc3de8324UL, 0xdac5b265UL,
    0x5d5daeaaUL, 0x44469febUL, 0x6f6bcc28UL, 0x7670fd69UL, 0x39316baeUL,
    0x202a5aefUL, 0x0b07092cUL, 0x121c386dUL, 0xdf4636f3UL, 0xc65d07b2UL,
    0xed705471UL, 0xf46b6530UL, 0xbb2af3f7UL, 0xa231c2b6UL, 0x891c9175UL,
    0x9007a034UL, 0x179fbcfbUL, 0x0e848dbaUL, 0x25a9de79UL, 0x3cb2ef38UL,
    0x73f379ffUL, 0x6ae848beUL, 0x41c51b7dUL, 0x58de2a3cUL, 0xf0794f05UL,
    0xe9627e44UL, 0xc24f2d87UL, 0xdb541cc6UL, 0x94158a01UL, 0x8d0ebb40UL,
    0xa623e883UL, 0xbf38d9c2UL, 0x38a0c50dUL, 0x21bbf44cUL, 0x0a96a78fUL,
    0x138d96ceUL, 0x5ccc0009UL, 0x45d73148UL, 0x6efa628bUL, 0x77e153caUL,
    0xbabb5d54UL, 0xa3a06c15UL, 0x888d3fd6UL, 0x91960e97UL, 0xded79850UL,
    0xc7cca911UL, 0xece1fad2UL, 0xf5facb93UL, 0x7262d75cUL, 0x6b79e61dUL,
    0x4054b5deUL, 0x594f849fUL, 0x160e1258UL, 0x0f152319UL, 0x243870daUL,
    0x3d23419bUL, 0x65fd6ba7UL, 0x7ce65ae6UL, 0x57cb0925UL, 0x4ed03864UL,
    0x0191aea3UL, 0x188a9fe2UL, 0x33a7cc21UL, 0x2abcfd60UL, 0xad24e1afUL,
    0xb43fd0eeUL, 0x9f12832dUL, 0x8609b26cUL, 0xc94824abUL, 0xd05315eaUL,
    0xfb7e4629UL, 0xe2657768UL, 0x2f3f79f6UL, 0x362448b7UL, 0x1d091b74UL,
    0x04122a35UL, 0x4b53bcf2UL, 0x52488db3UL, 0x7965de70UL, 0x607eef31UL,
    0xe7e6f3feUL, 0xfefdc2bfUL, 0xd5d0917cUL, 0xcccba03dUL, 0x838a36faUL,
    0x9a9107bbUL, 0xb1bc5478UL, 0xa8a76539UL, 0x3b83984bUL, 0x2298a90aUL,
    0x09b5fac9UL, 0x10aecb88UL, 0x5fef5d4fUL, 0x46f46c0eUL, 0x6dd93fcdUL,
    0x74c20e8cUL, 0xf35a1243UL, 0xea412302UL, 0xc16c70c1UL, 0xd8774180UL,
    0x9736d747UL, 0x8e2de606UL, 0xa500b5c5UL, 0xbc1b8484UL, 0x71418a1aUL,
    0x685abb5bUL, 0x4377e898UL, 0x5a6cd9d9UL, 0x152d4f1eUL, 0x0c367e5fUL,
    0x271b2d9cUL, 0x3e001cddUL, 0xb9980012UL, 0xa0833153UL, 0x8bae6290UL,
    0x92b553d1UL, 0xddf4c516UL, 0xc4eff457UL, 0xefc2a794UL, 0xf6d996d5UL,
    0xae07bce9UL, 0xb71c8da8UL, 0x9c31de6bUL, 0x852aef2aUL, 0xca6b79edUL,
    0xd37048acUL, 0xf85d1b6fUL, 0xe1462a2eUL, 0x66de36e1UL, 0x7fc507a0UL,
    0x54e85463UL, 0x4df36522UL, 0x02b2f3e5UL, 0x1ba9c2a4UL, 0x30849167UL,
    0x299fa026UL, 0xe4c5aeb8UL, 0xfdde9ff9UL, 0xd6f3cc3aUL, 0xcfe8fd7bUL,
    0x80a96bbcUL, 0x99b25afdUL, 0xb29f093eUL, 0xab84387fUL, 0x2c1c24b0UL,
    0x350715f1UL, 0x1e2a4632UL, 0x07317773UL, 0x4870e1b4UL, 0x516bd0f5UL,
    0x7a468336UL, 0x635db277UL, 0xcbfad74eUL, 0xd2e1e60fUL, 0xf9ccb5ccUL,
    0xe0d7848dUL, 0xaf96124aUL, 0xb68d230bUL, 0x9da070c8UL, 0x84bb4189UL,
    0x03235d46UL, 0x1a386c07UL, 0x31153fc4UL, 0x280e0e85UL, 0x674f9842UL,
    0x7e54a903UL, 0x5579fac0UL, 0x4c62cb81UL, 0x8138c51fUL, 0x9823f45eUL,
    0xb30ea79dUL, 0xaa1596dcUL, 0xe554001bUL, 0xfc4f315aUL, 0xd7626299UL,
    0xce7953d8UL, 0x49e14f17UL, 0x50fa7e56UL, 0x7bd72d95UL, 0x62cc1cd4UL,
    0x2d8d8a13UL, 0x3496bb52UL, 0x1fbbe891UL, 0x06a0d9d0UL, 0x5e7ef3ecUL,
    0x4765c2adUL, 0x6c48916eUL, 0x7553a02fUL, 0x3a1236e8UL, 0x230907a9UL,
    0x0824546aUL, 0x113f652bUL, 0x96a779e4UL, 0x8fbc48a5UL, 0xa4911b66UL,
    0xbd8a2a27UL, 0xf2cbbce0UL, 0xebd08da1UL, 0xc0fdde62UL, 0xd9e6ef23UL,
    0x14bce1bdUL, 0x0da7d0fcUL, 0x268a833fUL, 0x3f91b27eUL, 0x70d024b9UL,
    0x69cb15f8UL, 0x42e6463bUL, 0x5bfd777aUL, 0xdc656bb5UL, 0xc57e5af4UL,
    0xee530937UL, 0xf7483876UL, 0xb809aeb1UL, 0xa1129ff0UL, 0x8a3fcc33UL,
    0x9324fd72UL,

    // Table 2
    0x00000000UL, 0x01c26a37UL, 0x0384d46eUL, 0x0246be59UL, 0x0709a8dcUL,
    0x06cbc2ebUL, 0x048d7cb2UL, 0x054f1685UL, 0x0e1351b8UL, 0x0fd13b8fUL,
    0x0d9785d6UL, 0x0c55efe1UL, 0x091af964UL, 0x08d89353UL, 0x0a9e2d0aUL,
    0x0b5c473dUL, 0x1c26a370UL, 0x1de4c947UL, 0x1fa2771eUL, 0x1e601d29UL,
    0x1b2f0bacUL, 0x1aed619bUL, 0x18abdfc2UL, 0x1969b5f5UL, 0x1235f2c8UL,
    0x13f798ffUL, 0x11b126a6UL, 0x10734c91UL, 0x153c5a14UL, 0x14fe3023UL,
    0x16b88e7aUL, 0x177ae44dUL, 0x384d46e0UL, 0x398f2cd7UL, 0x3bc9928eUL,
    0x3a0bf8b9UL, 0x3f44ee3cUL, 0x3e86840bUL, 0x3cc03a52UL, 0x3d025065UL,
    0x365e1758UL, 0x379c7d6fUL, 0x35dac336UL, 0x3418a901UL, 0x3157bf84UL,
    0x3095d5b3UL, 0x32d36beaUL, 0x331101ddUL, 0x246be590UL, 0x25a98fa7UL,
    0x27ef31feUL, 0x262d5bc9UL, 0x23624d4cUL, 0x22a0277bUL, 0x20e69922UL,
    0x2124f315UL, 0x2a78b428UL, 0x2bbade1fUL, 0x29fc6046UL, 0x283e0a71UL,
    0x2d711cf4UL, 0x2cb376c3UL, 0x2ef5c89aUL, 0x2f37a2adUL, 0x709a8dc0UL,
    0x7158e7f7UL, 0x731e59aeUL, 0x72dc3399UL, 0x7793251cUL, 0x76514f2bUL,
    0x7417f172UL, 0x75d59b45UL, 0x7e89dc78UL, 0x7f4bb64fUL, 0x7d0d0816UL,
    0x7ccf6221UL, 0x798074a4UL, 0x78421e93UL, 0x7a04a0caUL, 0x7bc6cafdUL,
    0x6cbc2eb0UL, 0x6d7e4487UL, 0x6f38fadeUL, 0x6efa90e9UL, 0x6bb5866cUL,
    0x6a77ec5bUL, 0x68315202UL, 0x69f33835UL, 0x62af7f08UL, 0x636d153fUL,
    0x612bab66UL, 0x60e9c151UL, 0x65a6d7d4UL, 0x6464bde3UL, 0x662203baUL,
    0x67e0698dUL, 0x48d7cb20UL, 0x4915a117UL, 0x4b531f4eUL, 0x4a917579UL,
    0x4fde63fcUL, 0x4e1c09cbUL, 0x4c5ab792UL, 0x4d98dda5UL, 0x46c49a98UL,
    0x4706f0afUL, 0x45404ef6UL, 0x448224c1UL, 0x41cd3244UL, 0x400f5873UL,
    0x4249e62aUL, 0x438b8c1dUL, 0x54f16850UL, 0x55330267UL, 0x5775bc3eUL,
    0x56b7d609UL, 0x53f8c08cUL, 0x523aaabbUL, 0x507c14e2UL, 0x51be7ed5UL,
    0x5ae239e8UL, 0x5b2053dfUL, 0x5966ed86UL, 0x58a487b1UL, 0x5deb9134UL,
    0x5c29fb03UL, 0x5e6f455aUL, 0x5fad2f6dUL, 0xe1351b80UL, 0xe0f771b7UL,
    0xe2b1cfeeUL, 0xe373a5d9UL, 0xe63cb35cUL, 0xe7fed96bUL, 0xe5b86732UL,
    0xe47a0d05UL, 0xef264a38UL, 0xeee4200fUL, 0xeca29e56UL, 0xed60f461UL,
    0xe82fe2e4UL, 0xe9ed88d3UL, 0xebab368aUL, 0xea695cbdUL, 0xfd13b8f0UL,
    0xfcd1d2c7UL, 0xfe976c9eUL, 0xff5506a9UL, 0xfa1a102cUL, 0xfbd87a1bUL,
    0xf99ec442UL, 0xf85cae75UL, 0xf300e948UL, 0xf2c2837fUL, 0xf0843d26UL,
    0xf1465711UL, 0xf4094194UL, 0xf5cb2ba3UL, 0xf78d95faUL, 0xf64fffcdUL,
    0xd9785d60UL, 0xd8ba3757UL, 0xdafc890eUL, 0xdb3ee339UL, 0xde71f5bcUL,
    0xdfb39f8bUL, 0xddf521d2UL, 0xdc374be5UL, 0xd76b0cd8UL, 0xd6a966efUL,
    0xd4efd8b6UL, 0xd52db281UL, 0xd062a404UL, 0xd1a0ce33UL, 0xd3e6706aUL,
    0xd2241a5dUL, 0xc55efe10UL, 0xc49c9427UL, 0xc6da2a7eUL, 0xc7184049UL,
    0xc25756ccUL, 0xc3953cfbUL, 0xc1d382a2UL, 0xc011e895UL, 0xcb4dafa8UL,
    0xca8fc59fUL, 0xc8c97bc6UL, 0xc90b11f1UL, 0xcc440774UL, 0xcd866d43UL,
    0xcfc0d31aUL, 0xce02b92dUL, 0x91af9640UL, 0x906dfc77UL, 0x922b422eUL,
    0x93e92819UL, 0x96a63e9cUL, 0x976454abUL, 0x9522eaf2UL, 0x94e080c5UL,
    0x9fbcc7f8UL, 0x9e7eadcfUL, 0x9c381396UL, 0x9dfa79a1UL, 0x98b56f24UL,
    0x99770513UL, 0x9b31bb4aUL, 0x9af3d17dUL, 0x8d893530UL, 0x8c4b5f07UL,
    0x8e0de15eUL, 0x8fcf8b69UL, 0x8a809decUL, 0x8b42f7dbUL, 0x89044982UL,
    0x88c623b5UL, 0x839a6488UL, 0x82580ebfUL, 0x801eb0e6UL, 0x81dcdad1UL,
    0x8493cc54UL, 0x8551a663UL, 0x8717183aUL, 0x86d5720dUL, 0xa9e2d0a0UL,
    0xa820ba97UL, 0xaa6604ceUL, 0xaba46ef9UL, 0xaeeb787cUL, 0xaf29124bUL,
    0xad6fac12UL, 0xacadc625UL, 0xa7f18118UL, 0xa633eb2fUL, 0xa4755576UL,
    0xa5b73f41UL, 0xa0f829c4UL, 0xa13a43f3UL, 0xa37cfdaaUL, 0xa2be979dUL,
    0xb5c473d0UL, 0xb40619e7UL, 0xb640a7beUL, 0xb782cd89UL, 0xb2cddb0cUL,
    0xb30fb13bUL, 0xb1490f62UL, 0xb08b6555UL, 0xbbd72268UL, 0xba15485fUL,
    0xb853f606UL, 0xb9919c31UL, 0xbcde8ab4UL, 0xbd1ce083UL, 0xbf5a5edaUL,
    0xbe9834edUL,

    // Table 3
    0x00000000UL, 0xb8bc6765UL, 0xaa09c88bUL, 0x12b5afeeUL, 0x8f629757UL,
    0x37def032UL, 0x256b5fdcUL, 0x9dd738b9UL, 0xc5b428efUL, 0x7d084f8aUL,
    0x6fbde064UL, 0xd7018701UL, 0x4ad6bfb8UL, 0xf26ad8ddUL, 0xe0df7733UL,
    0x58631056UL, 0x5019579fUL, 0xe8a530faUL, 0xfa109f14UL, 0x42acf871UL,
    0xdf7bc0c8UL, 0x67c7a7adUL, 0x75720843UL, 0xcdce6f26UL, 0x95ad7f70UL,
    0x2d111815UL, 0x3fa4b7fbUL, 0x8718d09eUL, 0x1acfe827UL, 0xa2738f42UL,
    0xb0c620acUL, 0x087a47c9UL, 0xa032af3eUL, 0x188ec85bUL, 0x0a3b67b5UL,
    0xb28700d0UL, 0x2f503869UL, 0x97ec5f0cUL, 0x8559f0e2UL, 0x3de59787UL,
    0x658687d1UL, 0xdd3ae0b4UL, 0xcf8f4f5aUL, 0x7733283fUL, 0xeae41086UL,
    0x525877e3UL, 0x40edd80dUL, 0xf851bf68UL, 0xf02bf8a1UL, 0x48979fc4UL,
    0x5a22302aUL, 0xe29e574fUL, 0x7f496ff6UL, 0xc7f50893UL, 0xd540a77dUL,
    0x6dfcc018UL, 0x359fd04eUL, 0x8d23b72bUL, 0x9f9618c5UL, 0x272a7fa0UL,
    0xbafd4719UL, 0x0241207cUL, 0x10f48f92UL, 0xa848e8f7UL, 0x9b14583dUL,
    0x23a83f58UL, 0x311d90b6UL, 0x89a1f7d3UL, 0x1476cf6aUL, 0xaccaa80fUL,
    0xbe7f07e1UL, 0x06c36084UL, 0x5ea070d2UL, 0xe61c17b7UL, 0xf4a9b859UL,
    0x4c15df3cUL, 0xd1c2e785UL, 0x697e80e0UL, 0x7bcb2f0eUL, 0xc377486bUL,
    0xcb0d0fa2UL, 0x73b168c7UL, 0x6104c729UL, 0xd9b8a04cUL, 0x446f98f5UL,
    0xfcd3ff90UL, 0xee66507eUL, 0x56da371bUL, 0x0eb9274dUL, 0xb6054028UL,
    0xa4b0efc6UL, 0x1c0c88a3UL, 0x81dbb01aUL, 0x3967d77fUL, 0x2bd27891UL,
    0x936e1ff4UL, 0x3b26f703UL, 0x839a9066UL, 0x912f3f88UL, 0x299358edUL,
    0xb4446054UL, 0x0cf80731UL, 0x1e4da8dfUL, 0xa6f1cfbaUL, 0xfe92dfecUL,
    0x462eb889UL, 0x549b1767UL, 0xec277002UL, 0x71f048bbUL, 0xc94c2fdeUL,
    0xdbf98030UL, 0x6345e755UL, 0x6b3fa09cUL, 0xd383c7f9UL, 0xc1366817UL,
    0x798a0f72UL, 0xe45d37cbUL, 0x5ce150aeUL, 0x4e54ff40UL, 0xf6e89825UL,
    0xae8b8873UL, 0x1637ef16UL, 0x048240f8UL, 0xbc3e279dUL, 0x21e91f24UL,
    0x99557841UL, 0x8be0d7afUL, 0x335cb0caUL, 0xed59b63bUL, 0x55e5d15eUL,
    0x47507eb0UL, 0xffec19d5UL, 0x623b216cUL, 0xda874609UL, 0xc832e9e7UL,
    0x708e8e82UL, 0x28ed9ed4UL, 0x9051f9b1UL, 0x82e4565fUL, 0x3a58313aUL,
    0xa78f0983UL, 0x1f336ee6UL, 0x0d86c108UL, 0xb53aa66dUL, 0xbd40e1a4UL,
    0x05fc86c1UL, 0x1749292fUL, 0xaff54e4aUL, 0x322276f3UL, 0x8a9e1196UL,
    0x982bbe78UL, 0x2097d91dUL, 0x78f4c94bUL, 0xc048ae2eUL, 0xd2fd01c0UL,
    0x6a4166a5UL, 0xf7965e1cUL, 0x4f2a3979UL, 0x5d9f9697UL, 0xe523f1f2UL,
    0x4d6b1905UL, 0xf5d77e60UL, 0xe762d18eUL, 0x5fdeb6ebUL, 0xc2098e52UL,
    0x7ab5e937UL, 0x680046d9UL, 0xd0bc21bcUL, 0x88df31eaUL, 0x3063568fUL,
    0x22d6f961UL, 0x9a6a9e04UL, 0x07bda6bdUL, 0xbf01c1d8UL, 0xadb46e36UL,
    0x15080953UL, 0x1d724e9aUL, 0xa5ce29ffUL, 0xb77b8611UL, 0x0fc7e174UL,
    0x9210d9cdUL, 0x2aacbea8UL, 0x38191146UL, 0x80a57623UL, 0xd8c66675UL,
    0x607a0110UL, 0x72cfaefeUL, 0xca73c99bUL, 0x57a4f122UL, 0xef189647UL,
    0xfdad39a9UL, 0x45115eccUL, 0x764dee06UL, 0xcef18963UL, 0xdc44268dUL,
    0x64f841e8UL, 0xf92f7951UL, 0x41931e34UL, 0x5326b1daUL, 0xeb9ad6bfUL,
    0xb3f9c6e9UL, 0x0b45a18cUL, 0x19f00e62UL, 0xa14c6907UL, 0x3c9b51beUL,
    0x842736dbUL, 0x96929935UL, 0x2e2efe50UL, 0x2654b999UL, 0x9ee8defcUL,
    0x8c5d7112UL, 0x34e11677UL, 0xa9362eceUL, 0x118a49abUL, 0x033fe645UL,
    0xbb838120UL, 0xe3e09176UL, 0x5b5cf613UL, 0x49e959fdUL, 0xf1553e98UL,
    0x6c820621UL, 0xd43e6144UL, 0xc68bceaaUL, 0x7e37a9cfUL, 0xd67f4138UL,
    0x6ec3265dUL, 0x7c7689b3UL, 0xc4caeed6UL, 0x591dd66fUL, 0xe1a1b10aUL,
    0xf3141ee4UL, 0x4ba87981UL, 0x13cb69d7UL, 0xab770eb2UL, 0xb9c2a15cUL,
    0x017ec639UL, 0x9ca9fe80UL, 0x241599e5UL, 0x36a0360bUL, 0x8e1c516eUL,
    0x866616a7UL, 0x3eda71c2UL, 0x2c6fde2cUL, 0x94d3b949UL, 0x090481f0UL,
    0xb1b8e695UL, 0xa30d497bUL, 0x1bb12e1eUL, 0x43d23e48UL, 0xfb6e592dUL,
    0xe9dbf6c3UL, 0x516791a6UL, 0xccb0a91fUL, 0x740cce7aUL, 0x66b96194UL,
    0xde0506f1UL,

    // Tables for vector version
    // This improvement (vectorization) is based on java.base/share/native/libzip/zlib/zcrc32.c.
    // To make it, following steps are taken:
    //  1. in zcrc32.c, modify N to 16 and related code,
    //  2. re-generate the tables needed, we use tables of (N == 16, W == 4)
    //  3. finally vectorize the code (original implementation in zcrc32.c is just scalar code).
    0x00000000, 0x8f352d95, 0xc51b5d6b, 0x4a2e70fe, 0x5147bc97,
    0xde729102, 0x945ce1fc, 0x1b69cc69, 0xa28f792e, 0x2dba54bb,
    0x67942445, 0xe8a109d0, 0xf3c8c5b9, 0x7cfde82c, 0x36d398d2,
    0xb9e6b547, 0x9e6ff41d, 0x115ad988, 0x5b74a976, 0xd44184e3,
    0xcf28488a, 0x401d651f, 0x0a3315e1, 0x85063874, 0x3ce08d33,
    0xb3d5a0a6, 0xf9fbd058, 0x76cefdcd, 0x6da731a4, 0xe2921c31,
    0xa8bc6ccf, 0x2789415a, 0xe7aeee7b, 0x689bc3ee, 0x22b5b310,
    0xad809e85, 0xb6e952ec, 0x39dc7f79, 0x73f20f87, 0xfcc72212,
    0x45219755, 0xca14bac0, 0x803aca3e, 0x0f0fe7ab, 0x14662bc2,
    0x9b530657, 0xd17d76a9, 0x5e485b3c, 0x79c11a66, 0xf6f437f3,
    0xbcda470d, 0x33ef6a98, 0x2886a6f1, 0xa7b38b64, 0xed9dfb9a,
    0x62a8d60f, 0xdb4e6348, 0x547b4edd, 0x1e553e23, 0x916013b6,
    0x8a09dfdf, 0x053cf24a, 0x4f1282b4, 0xc027af21, 0x142cdab7,
    0x9b19f722, 0xd13787dc, 0x5e02aa49, 0x456b6620, 0xca5e4bb5,
    0x80703b4b, 0x0f4516de, 0xb6a3a399, 0x39968e0c, 0x73b8fef2,
    0xfc8dd367, 0xe7e41f0e, 0x68d1329b, 0x22ff4265, 0xadca6ff0,
    0x8a432eaa, 0x0576033f, 0x4f5873c1, 0xc06d5e54, 0xdb04923d,
    0x5431bfa8, 0x1e1fcf56, 0x912ae2c3, 0x28cc5784, 0xa7f97a11,
    0xedd70aef, 0x62e2277a, 0x798beb13, 0xf6bec686, 0xbc90b678,
    0x33a59bed, 0xf38234cc, 0x7cb71959, 0x369969a7, 0xb9ac4432,
    0xa2c5885b, 0x2df0a5ce, 0x67ded530, 0xe8ebf8a5, 0x510d4de2,
    0xde386077, 0x94161089, 0x1b233d1c, 0x004af175, 0x8f7fdce0,
    0xc551ac1e, 0x4a64818b, 0x6dedc0d1, 0xe2d8ed44, 0xa8f69dba,
    0x27c3b02f, 0x3caa7c46, 0xb39f51d3, 0xf9b1212d, 0x76840cb8,
    0xcf62b9ff, 0x4057946a, 0x0a79e494, 0x854cc901, 0x9e250568,
    0x111028fd, 0x5b3e5803, 0xd40b7596, 0x2859b56e, 0xa76c98fb,
    0xed42e805, 0x6277c590, 0x791e09f9, 0xf62b246c, 0xbc055492,
    0x33307907, 0x8ad6cc40, 0x05e3e1d5, 0x4fcd912b, 0xc0f8bcbe,
    0xdb9170d7, 0x54a45d42, 0x1e8a2dbc, 0x91bf0029, 0xb6364173,
    0x39036ce6, 0x732d1c18, 0xfc18318d, 0xe771fde4, 0x6844d071,
    0x226aa08f, 0xad5f8d1a, 0x14b9385d, 0x9b8c15c8, 0xd1a26536,
    0x5e9748a3, 0x45fe84ca, 0xcacba95f, 0x80e5d9a1, 0x0fd0f434,
    0xcff75b15, 0x40c27680, 0x0aec067e, 0x85d92beb, 0x9eb0e782,
    0x1185ca17, 0x5babbae9, 0xd49e977c, 0x6d78223b, 0xe24d0fae,
    0xa8637f50, 0x275652c5, 0x3c3f9eac, 0xb30ab339, 0xf924c3c7,
    0x7611ee52, 0x5198af08, 0xdead829d, 0x9483f263, 0x1bb6dff6,
    0x00df139f, 0x8fea3e0a, 0xc5c44ef4, 0x4af16361, 0xf317d626,
    0x7c22fbb3, 0x360c8b4d, 0xb939a6d8, 0xa2506ab1, 0x2d654724,
    0x674b37da, 0xe87e1a4f, 0x3c756fd9, 0xb340424c, 0xf96e32b2,
    0x765b1f27, 0x6d32d34e, 0xe207fedb, 0xa8298e25, 0x271ca3b0,
    0x9efa16f7, 0x11cf3b62, 0x5be14b9c, 0xd4d46609, 0xcfbdaa60,
    0x408887f5, 0x0aa6f70b, 0x8593da9e, 0xa21a9bc4, 0x2d2fb651,
    0x6701c6af, 0xe834eb3a, 0xf35d2753, 0x7c680ac6, 0x36467a38,
    0xb97357ad, 0x0095e2ea, 0x8fa0cf7f, 0xc58ebf81, 0x4abb9214,
    0x51d25e7d, 0xdee773e8, 0x94c90316, 0x1bfc2e83, 0xdbdb81a2,
    0x54eeac37, 0x1ec0dcc9, 0x91f5f15c, 0x8a9c3d35, 0x05a910a0,
    0x4f87605e, 0xc0b24dcb, 0x7954f88c, 0xf661d519, 0xbc4fa5e7,
    0x337a8872, 0x2813441b, 0xa726698e, 0xed081970, 0x623d34e5,
    0x45b475bf, 0xca81582a, 0x80af28d4, 0x0f9a0541, 0x14f3c928,
    0x9bc6e4bd, 0xd1e89443, 0x5eddb9d6, 0xe73b0c91, 0x680e2104,
    0x222051fa, 0xad157c6f, 0xb67cb006, 0x39499d93, 0x7367ed6d,
    0xfc52c0f8,
    0x00000000, 0x50b36adc, 0xa166d5b8, 0xf1d5bf64, 0x99bcad31,
    0xc90fc7ed, 0x38da7889, 0x68691255, 0xe8085c23, 0xb8bb36ff,
    0x496e899b, 0x19dde347, 0x71b4f112, 0x21079bce, 0xd0d224aa,
    0x80614e76, 0x0b61be07, 0x5bd2d4db, 0xaa076bbf, 0xfab40163,
    0x92dd1336, 0xc26e79ea, 0x33bbc68e, 0x6308ac52, 0xe369e224,
    0xb3da88f8, 0x420f379c, 0x12bc5d40, 0x7ad54f15, 0x2a6625c9,
    0xdbb39aad, 0x8b00f071, 0x16c37c0e, 0x467016d2, 0xb7a5a9b6,
    0xe716c36a, 0x8f7fd13f, 0xdfccbbe3, 0x2e190487, 0x7eaa6e5b,
    0xfecb202d, 0xae784af1, 0x5fadf595, 0x0f1e9f49, 0x67778d1c,
    0x37c4e7c0, 0xc61158a4, 0x96a23278, 0x1da2c209, 0x4d11a8d5,
    0xbcc417b1, 0xec777d6d, 0x841e6f38, 0xd4ad05e4, 0x2578ba80,
    0x75cbd05c, 0xf5aa9e2a, 0xa519f4f6, 0x54cc4b92, 0x047f214e,
    0x6c16331b, 0x3ca559c7, 0xcd70e6a3, 0x9dc38c7f, 0x2d86f81c,
    0x7d3592c0, 0x8ce02da4, 0xdc534778, 0xb43a552d, 0xe4893ff1,
    0x155c8095, 0x45efea49, 0xc58ea43f, 0x953dcee3, 0x64e87187,
    0x345b1b5b, 0x5c32090e, 0x0c8163d2, 0xfd54dcb6, 0xade7b66a,
    0x26e7461b, 0x76542cc7, 0x878193a3, 0xd732f97f, 0xbf5beb2a,
    0xefe881f6, 0x1e3d3e92, 0x4e8e544e, 0xceef1a38, 0x9e5c70e4,
    0x6f89cf80, 0x3f3aa55c, 0x5753b709, 0x07e0ddd5, 0xf63562b1,
    0xa686086d, 0x3b458412, 0x6bf6eece, 0x9a2351aa, 0xca903b76,
    0xa2f92923, 0xf24a43ff, 0x039ffc9b, 0x532c9647, 0xd34dd831,
    0x83feb2ed, 0x722b0d89, 0x22986755, 0x4af17500, 0x1a421fdc,
    0xeb97a0b8, 0xbb24ca64, 0x30243a15, 0x609750c9, 0x9142efad,
    0xc1f18571, 0xa9989724, 0xf92bfdf8, 0x08fe429c, 0x584d2840,
    0xd82c6636, 0x889f0cea, 0x794ab38e, 0x29f9d952, 0x4190cb07,
    0x1123a1db, 0xe0f61ebf, 0xb0457463, 0x5b0df038, 0x0bbe9ae4,
    0xfa6b2580, 0xaad84f5c, 0xc2b15d09, 0x920237d5, 0x63d788b1,
    0x3364e26d, 0xb305ac1b, 0xe3b6c6c7, 0x126379a3, 0x42d0137f,
    0x2ab9012a, 0x7a0a6bf6, 0x8bdfd492, 0xdb6cbe4e, 0x506c4e3f,
    0x00df24e3, 0xf10a9b87, 0xa1b9f15b, 0xc9d0e30e, 0x996389d2,
    0x68b636b6, 0x38055c6a, 0xb864121c, 0xe8d778c0, 0x1902c7a4,
    0x49b1ad78, 0x21d8bf2d, 0x716bd5f1, 0x80be6a95, 0xd00d0049,
    0x4dce8c36, 0x1d7de6ea, 0xeca8598e, 0xbc1b3352, 0xd4722107,
    0x84c14bdb, 0x7514f4bf, 0x25a79e63, 0xa5c6d015, 0xf575bac9,
    0x04a005ad, 0x54136f71, 0x3c7a7d24, 0x6cc917f8, 0x9d1ca89c,
    0xcdafc240, 0x46af3231, 0x161c58ed, 0xe7c9e789, 0xb77a8d55,
    0xdf139f00, 0x8fa0f5dc, 0x7e754ab8, 0x2ec62064, 0xaea76e12,
    0xfe1404ce, 0x0fc1bbaa, 0x5f72d176, 0x371bc323, 0x67a8a9ff,
    0x967d169b, 0xc6ce7c47, 0x768b0824, 0x263862f8, 0xd7eddd9c,
    0x875eb740, 0xef37a515, 0xbf84cfc9, 0x4e5170ad, 0x1ee21a71,
    0x9e835407, 0xce303edb, 0x3fe581bf, 0x6f56eb63, 0x073ff936,
    0x578c93ea, 0xa6592c8e, 0xf6ea4652, 0x7deab623, 0x2d59dcff,
    0xdc8c639b, 0x8c3f0947, 0xe4561b12, 0xb4e571ce, 0x4530ceaa,
    0x1583a476, 0x95e2ea00, 0xc55180dc, 0x34843fb8, 0x64375564,
    0x0c5e4731, 0x5ced2ded, 0xad389289, 0xfd8bf855, 0x6048742a,
    0x30fb1ef6, 0xc12ea192, 0x919dcb4e, 0xf9f4d91b, 0xa947b3c7,
    0x58920ca3, 0x0821667f, 0x88402809, 0xd8f342d5, 0x2926fdb1,
    0x7995976d, 0x11fc8538, 0x414fefe4, 0xb09a5080, 0xe0293a5c,
    0x6b29ca2d, 0x3b9aa0f1, 0xca4f1f95, 0x9afc7549, 0xf295671c,
    0xa2260dc0, 0x53f3b2a4, 0x0340d878, 0x8321960e, 0xd392fcd2,
    0x224743b6, 0x72f4296a, 0x1a9d3b3f, 0x4a2e51e3, 0xbbfbee87,
    0xeb48845b,
    0x00000000, 0xb61be070, 0xb746c6a1, 0x015d26d1, 0xb5fc8b03,
    0x03e76b73, 0x02ba4da2, 0xb4a1add2, 0xb0881047, 0x0693f037,
    0x07ced6e6, 0xb1d53696, 0x05749b44, 0xb36f7b34, 0xb2325de5,
    0x0429bd95, 0xba6126cf, 0x0c7ac6bf, 0x0d27e06e, 0xbb3c001e,
    0x0f9dadcc, 0xb9864dbc, 0xb8db6b6d, 0x0ec08b1d, 0x0ae93688,
    0xbcf2d6f8, 0xbdaff029, 0x0bb41059, 0xbf15bd8b, 0x090e5dfb,
    0x08537b2a, 0xbe489b5a, 0xafb34bdf, 0x19a8abaf, 0x18f58d7e,
    0xaeee6d0e, 0x1a4fc0dc, 0xac5420ac, 0xad09067d, 0x1b12e60d,
    0x1f3b5b98, 0xa920bbe8, 0xa87d9d39, 0x1e667d49, 0xaac7d09b,
    0x1cdc30eb, 0x1d81163a, 0xab9af64a, 0x15d26d10, 0xa3c98d60,
    0xa294abb1, 0x148f4bc1, 0xa02ee613, 0x16350663, 0x176820b2,
    0xa173c0c2, 0xa55a7d57, 0x13419d27, 0x121cbbf6, 0xa4075b86,
    0x10a6f654, 0xa6bd1624, 0xa7e030f5, 0x11fbd085, 0x841791ff,
    0x320c718f, 0x3351575e, 0x854ab72e, 0x31eb1afc, 0x87f0fa8c,
    0x86addc5d, 0x30b63c2d, 0x349f81b8, 0x828461c8, 0x83d94719,
    0x35c2a769, 0x81630abb, 0x3778eacb, 0x3625cc1a, 0x803e2c6a,
    0x3e76b730, 0x886d5740, 0x89307191, 0x3f2b91e1, 0x8b8a3c33,
    0x3d91dc43, 0x3cccfa92, 0x8ad71ae2, 0x8efea777, 0x38e54707,
    0x39b861d6, 0x8fa381a6, 0x3b022c74, 0x8d19cc04, 0x8c44ead5,
    0x3a5f0aa5, 0x2ba4da20, 0x9dbf3a50, 0x9ce21c81, 0x2af9fcf1,
    0x9e585123, 0x2843b153, 0x291e9782, 0x9f0577f2, 0x9b2cca67,
    0x2d372a17, 0x2c6a0cc6, 0x9a71ecb6, 0x2ed04164, 0x98cba114,
    0x999687c5, 0x2f8d67b5, 0x91c5fcef, 0x27de1c9f, 0x26833a4e,
    0x9098da3e, 0x243977ec, 0x9222979c, 0x937fb14d, 0x2564513d,
    0x214deca8, 0x97560cd8, 0x960b2a09, 0x2010ca79, 0x94b167ab,
    0x22aa87db, 0x23f7a10a, 0x95ec417a, 0xd35e25bf, 0x6545c5cf,
    0x6418e31e, 0xd203036e, 0x66a2aebc, 0xd0b94ecc, 0xd1e4681d,
    0x67ff886d, 0x63d635f8, 0xd5cdd588, 0xd490f359, 0x628b1329,
    0xd62abefb, 0x60315e8b, 0x616c785a, 0xd777982a, 0x693f0370,
    0xdf24e300, 0xde79c5d1, 0x686225a1, 0xdcc38873, 0x6ad86803,
    0x6b854ed2, 0xdd9eaea2, 0xd9b71337, 0x6facf347, 0x6ef1d596,
    0xd8ea35e6, 0x6c4b9834, 0xda507844, 0xdb0d5e95, 0x6d16bee5,
    0x7ced6e60, 0xcaf68e10, 0xcbaba8c1, 0x7db048b1, 0xc911e563,
    0x7f0a0513, 0x7e5723c2, 0xc84cc3b2, 0xcc657e27, 0x7a7e9e57,
    0x7b23b886, 0xcd3858f6, 0x7999f524, 0xcf821554, 0xcedf3385,
    0x78c4d3f5, 0xc68c48af, 0x7097a8df, 0x71ca8e0e, 0xc7d16e7e,
    0x7370c3ac, 0xc56b23dc, 0xc436050d, 0x722de57d, 0x760458e8,
    0xc01fb898, 0xc1429e49, 0x77597e39, 0xc3f8d3eb, 0x75e3339b,
    0x74be154a, 0xc2a5f53a, 0x5749b440, 0xe1525430, 0xe00f72e1,
    0x56149291, 0xe2b53f43, 0x54aedf33, 0x55f3f9e2, 0xe3e81992,
    0xe7c1a407, 0x51da4477, 0x508762a6, 0xe69c82d6, 0x523d2f04,
    0xe426cf74, 0xe57be9a5, 0x536009d5, 0xed28928f, 0x5b3372ff,
    0x5a6e542e, 0xec75b45e, 0x58d4198c, 0xeecff9fc, 0xef92df2d,
    0x59893f5d, 0x5da082c8, 0xebbb62b8, 0xeae64469, 0x5cfda419,
    0xe85c09cb, 0x5e47e9bb, 0x5f1acf6a, 0xe9012f1a, 0xf8faff9f,
    0x4ee11fef, 0x4fbc393e, 0xf9a7d94e, 0x4d06749c, 0xfb1d94ec,
    0xfa40b23d, 0x4c5b524d, 0x4872efd8, 0xfe690fa8, 0xff342979,
    0x492fc909, 0xfd8e64db, 0x4b9584ab, 0x4ac8a27a, 0xfcd3420a,
    0x429bd950, 0xf4803920, 0xf5dd1ff1, 0x43c6ff81, 0xf7675253,
    0x417cb223, 0x402194f2, 0xf63a7482, 0xf213c917, 0x44082967,
    0x45550fb6, 0xf34eefc6, 0x47ef4214, 0xf1f4a264, 0xf0a984b5,
    0x46b264c5,
    0x00000000, 0x7dcd4d3f, 0xfb9a9a7e, 0x8657d741, 0x2c4432bd,
    0x51897f82, 0xd7dea8c3, 0xaa13e5fc, 0x5888657a, 0x25452845,
    0xa312ff04, 0xdedfb23b, 0x74cc57c7, 0x09011af8, 0x8f56cdb9,
    0xf29b8086, 0xb110caf4, 0xccdd87cb, 0x4a8a508a, 0x37471db5,
    0x9d54f849, 0xe099b576, 0x66ce6237, 0x1b032f08, 0xe998af8e,
    0x9455e2b1, 0x120235f0, 0x6fcf78cf, 0xc5dc9d33, 0xb811d00c,
    0x3e46074d, 0x438b4a72, 0xb95093a9, 0xc49dde96, 0x42ca09d7,
    0x3f0744e8, 0x9514a114, 0xe8d9ec2b, 0x6e8e3b6a, 0x13437655,
    0xe1d8f6d3, 0x9c15bbec, 0x1a426cad, 0x678f2192, 0xcd9cc46e,
    0xb0518951, 0x36065e10, 0x4bcb132f, 0x0840595d, 0x758d1462,
    0xf3dac323, 0x8e178e1c, 0x24046be0, 0x59c926df, 0xdf9ef19e,
    0xa253bca1, 0x50c83c27, 0x2d057118, 0xab52a659, 0xd69feb66,
    0x7c8c0e9a, 0x014143a5, 0x871694e4, 0xfadbd9db, 0xa9d02113,
    0xd41d6c2c, 0x524abb6d, 0x2f87f652, 0x859413ae, 0xf8595e91,
    0x7e0e89d0, 0x03c3c4ef, 0xf1584469, 0x8c950956, 0x0ac2de17,
    0x770f9328, 0xdd1c76d4, 0xa0d13beb, 0x2686ecaa, 0x5b4ba195,
    0x18c0ebe7, 0x650da6d8, 0xe35a7199, 0x9e973ca6, 0x3484d95a,
    0x49499465, 0xcf1e4324, 0xb2d30e1b, 0x40488e9d, 0x3d85c3a2,
    0xbbd214e3, 0xc61f59dc, 0x6c0cbc20, 0x11c1f11f, 0x9796265e,
    0xea5b6b61, 0x1080b2ba, 0x6d4dff85, 0xeb1a28c4, 0x96d765fb,
    0x3cc48007, 0x4109cd38, 0xc75e1a79, 0xba935746, 0x4808d7c0,
    0x35c59aff, 0xb3924dbe, 0xce5f0081, 0x644ce57d, 0x1981a842,
    0x9fd67f03, 0xe21b323c, 0xa190784e, 0xdc5d3571, 0x5a0ae230,
    0x27c7af0f, 0x8dd44af3, 0xf01907cc, 0x764ed08d, 0x0b839db2,
    0xf9181d34, 0x84d5500b, 0x0282874a, 0x7f4fca75, 0xd55c2f89,
    0xa89162b6, 0x2ec6b5f7, 0x530bf8c8, 0x88d14467, 0xf51c0958,
    0x734bde19, 0x0e869326, 0xa49576da, 0xd9583be5, 0x5f0feca4,
    0x22c2a19b, 0xd059211d, 0xad946c22, 0x2bc3bb63, 0x560ef65c,
    0xfc1d13a0, 0x81d05e9f, 0x078789de, 0x7a4ac4e1, 0x39c18e93,
    0x440cc3ac, 0xc25b14ed, 0xbf9659d2, 0x1585bc2e, 0x6848f111,
    0xee1f2650, 0x93d26b6f, 0x6149ebe9, 0x1c84a6d6, 0x9ad37197,
    0xe71e3ca8, 0x4d0dd954, 0x30c0946b, 0xb697432a, 0xcb5a0e15,
    0x3181d7ce, 0x4c4c9af1, 0xca1b4db0, 0xb7d6008f, 0x1dc5e573,
    0x6008a84c, 0xe65f7f0d, 0x9b923232, 0x6909b2b4, 0x14c4ff8b,
    0x929328ca, 0xef5e65f5, 0x454d8009, 0x3880cd36, 0xbed71a77,
    0xc31a5748, 0x80911d3a, 0xfd5c5005, 0x7b0b8744, 0x06c6ca7b,
    0xacd52f87, 0xd11862b8, 0x574fb5f9, 0x2a82f8c6, 0xd8197840,
    0xa5d4357f, 0x2383e23e, 0x5e4eaf01, 0xf45d4afd, 0x899007c2,
    0x0fc7d083, 0x720a9dbc, 0x21016574, 0x5ccc284b, 0xda9bff0a,
    0xa756b235, 0x0d4557c9, 0x70881af6, 0xf6dfcdb7, 0x8b128088,
    0x7989000e, 0x04444d31, 0x82139a70, 0xffded74f, 0x55cd32b3,
    0x28007f8c, 0xae57a8cd, 0xd39ae5f2, 0x9011af80, 0xeddce2bf,
    0x6b8b35fe, 0x164678c1, 0xbc559d3d, 0xc198d002, 0x47cf0743,
    0x3a024a7c, 0xc899cafa, 0xb55487c5, 0x33035084, 0x4ece1dbb,
    0xe4ddf847, 0x9910b578, 0x1f476239, 0x628a2f06, 0x9851f6dd,
    0xe59cbbe2, 0x63cb6ca3, 0x1e06219c, 0xb415c460, 0xc9d8895f,
    0x4f8f5e1e, 0x32421321, 0xc0d993a7, 0xbd14de98, 0x3b4309d9,
    0x468e44e6, 0xec9da11a, 0x9150ec25, 0x17073b64, 0x6aca765b,
    0x29413c29, 0x548c7116, 0xd2dba657, 0xaf16eb68, 0x05050e94,
    0x78c843ab, 0xfe9f94ea, 0x8352d9d5, 0x71c95953, 0x0c04146c,
    0x8a53c32d, 0xf79e8e12, 0x5d8d6bee, 0x204026d1, 0xa617f190,
    0xdbdabcaf,

    // CRC32 table for carry-less multiplication implementation
    0xe88ef372UL, 0x00000001UL,
    0x4a7fe880UL, 0x00000001UL,
    0x54442bd4UL, 0x00000001UL,
    0xc6e41596UL, 0x00000001UL,
    0x3db1ecdcUL, 0x00000000UL,
    0x74359406UL, 0x00000001UL,
    0xf1da05aaUL, 0x00000000UL,
    0x5a546366UL, 0x00000001UL,
    0x751997d0UL, 0x00000001UL,
    0xccaa009eUL, 0x00000000UL,
};
