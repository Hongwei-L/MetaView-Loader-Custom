# Meta View XR SDK Package

<!--
Copyright (c) 2020, Meta View, Inc.

All rights reserved.
SPDX-License-Identifier: UNLICENSED
-->

This package provides direct-mode rendering for Meta View headsets on Windows 10
2004 and newer, through the Unity XR Plugin SDK.

For the purposes of this plugin, a "Meta View headset" is a display meeting both
of the following criteria:

- The PNPID used in the EDID is `CFR` or `MVA`.
- The CTA extension in the EDID contains the
  [Microsoft HMD vendor-specific data block (VSDB)][edid-ext], version 2, with
  primary product use case 0x7 ("Virtual reality headsets"). That is, the first
  two payload bytes of the block are 0x02, 0x07.

[edid-ext]: https://docs.microsoft.com/en-us/windows-hardware/drivers/display/specialized-monitors-edid-extension

It is based, in part, on the Valve SteamVR/OpenVR [unity-xr-plugin][], used
under the 3-clause BSD license, as well as the Microsoft
[DisplayCoreCustomCompositor][] sample available under the MIT license. It
bundles and depends on [DirectXTK for DX11][], August 2020 release, also
available under the MIT license. Additionally, it uses the Unity headers in the
`CommonHeaders/ProviderInterface` folder, where you'll find the Unity Companion
License
([/CommonHeaders/ProviderInterface/LICENSE.md](CommonHeaders/ProviderInterface/LICENSE.md)).
Where possible, SPDX license identifier metadata tags have been added, with
additional copyright/license data added as needed to the `.reuse/dep5` file per
the [reuse.software](https://reuse.software/) specification. (It does not comply
with that open-source focused specification entirely or pass `reuse lint`
because code written or modified expressly for this project is labeled as
"UNLICENSED", which is not a recognized open-source license identifier.)

[unity-xr-plugin]: https://github.com/ValveSoftware/unity-xr-plugin
[DisplayCoreCustomCompositor]: https://github.com/microsoft/Windows-classic-samples/tree/master/Samples/DisplayCoreCustomCompositor
[DirectXTK for DX11]: https://github.com/microsoft/DirectXTK
