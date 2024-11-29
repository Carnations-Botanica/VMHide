<h1 align="center">VMHide</h1>

<h5 align="center">Grants the ability to control Darwin's knowledge of VMM Presence.</h5>
</br>

A [Lilu](https://github.com/acidanthera/Lilu) plug-in modeled after [ECEnabler]() and [RestrictEvents](https://github.com/1revenger1/ECEnabler) which resolves ``_sysctl__children`` to get the ``sysctl_oid_list`` for ``hv_vmm_present`` in the ``kern`` node, allowing VMHide to reroute it to a custom function: ``vmh_sysctl_vmm_present`` which selectively returns VMM Presence depending on a filtered list of known processes to hide from.

</br>
<h1 align="center">Purpose</h1>
</br>

This kernel extension was developed specifically for macOS 15 Sequoia but has been tested from Sonoma (although not required) to Sequoia (15.2) with the intention of fixing a side effect of [the following new implementation for Xcode/Hypervisor.framework macOS guests on M-Series hardware supporting limited iCloud support](https://support.apple.com/en-us/120468), which can be seen in [Apple's Developer Documentation](https://developer.apple.com/documentation/virtualization/using_icloud_with_macos_virtual_machines#4412628) as well.

- Hides VMM presence from various Apple ID related processes, sysctl, and the kernel.

- Utilizes Carnation's first ProjectExtension [Log2Disk](https://github.com/Carnations-Botanica/ProjectExtensions) to provide easy bug reporting.

- Source code contains a visible list that can easily be updated and PR'd to add more.

</br>
<h1 align="center">Usage / Features</h1>
</br>

### Usage

**To use VMHide, you must be using [the latest *DEBUG* version of Lilu](https://github.com/acidanthera/Lilu/releases/download/1.6.9/Lilu-1.6.9-DEBUG.zip) (1.6.9+ required) to properly load the plug-in. This is a currently known *bug*, and will be resolved in a timely manner when possible. As a note, requirement of DEBUG is not intentional so it's currently a bug of VMHide.**

### Features

VMHide has a few set of possible states it can be set to. **By default, simply dropping the Kernel Extension in your kext folder and updating config.plist with ProperTree, will automatically hide VMM status if you are on a virtual machine.** To specifically set the state for **debugging** and **contributors testing code** you can add a boot argument with the following options:

``vmhState`` - Accepts the intended state of action.

- ``enabled`` -> Force hiding VMM Status. Bypasses actual VM requirement during initial boot.
- ``disabled`` -> Force enabling VMM Status. On non-hypervisors, force spoofing as one.
- ``passthrough`` -> Placeholder option for forcing/spoofing VMM status, does nothing at the time of writing.

</br>

### Debugging, Bug Reporting, Contributing to Filter.

If you find that you're running into issues that must be reported, or wish to contribute to the list of processes that should not be VM-aware, you can use Log2Disk's support to write a local log file with information from VMHide written to easily read within macOS and for sharing the log. 

Please note that at the time of writing this information, the inclusion of L2D has not been made public, and the following boot arguments will not do anything. This information exists for future usage and as a placeholder.

``l2dEnable`` - One way switch, having this argument enables Log2Disk. 

</br>

``l2dLogLevel`` - Set the Log2Disk verbosity level.

- ``error`` -> Only log ERROR messages to disk.
- ``warning`` -> Only log WARNING messages to disk.
- ``all`` -> Log all messages, even simple ones.

</br>
<b>Example boot-args for Developers/Contributors (This is not required to use VMHide)</b>

```bash
-l2dEnable vmhState=enabled l2dLogLevel=all
```

</br>
<h1 align="center">Contributing to the Project</h1>

<h4 align="center">If you have any changes or improvements you'd like to contribute for review and merge, to update conventional mistakes or for QoL, as well as maybe even adding whole new features, you can follow the general outline below to get a local copy of the source building.</h4>

</br>

1. Install/Update ``Xcode``
    - Visit https://xcodereleases.com/ for your appropriate latest version.

2. Prepare source code
    - ``git clone https://github.com/Carnations-Botanica/VMHide.git``
    - ``cd VMHide``
    - ``git clone https://github.com/acidanthera/MacKernelSDK``
    - Get the latest ``DEBUG`` Lilu.kext from [Releases](https://github.com/acidanthera/Lilu/releases) and place in the root of the repo. Example required placement is below.
        - VMHide/VMHide.xcodeproj <- Xcode Project file.
        - VMHide/VMHide/ <- Project Contents.
        - VMHide/MacKernelSDK <- ``git`` cloned from a Terminal.
        - VMHide/Lilu.kext <- Actual .kext file here.
        - VMHide/README.md <- How you can tell you're in the root.

3. Launch ``.xcodeproj`` with Xcode to begin!
    - ``kern_start.cpp`` - Contains functions such as ``vmh_sysctl_vmm_present``.
    - ``kern_start.hpp`` - Header for Main, sets up various macros and globals.

<br>
<h1 align="center">Special Thanks!</h1>
<br>

[<b>1Revenger1</b>](https://github.com/1revenger1) - Took the time to explain how to develop kernel extensions from scratch, continues to provide ways to improve the source code of VMHide! They have been credited as a contributor with the ``Initial Commit``.

[<b>ECEnabler</b>](https://github.com/1Revenger1/ECEnabler) - Served as the basis for the very first attempts building for macOS 15 and testing ideas in removed commits.

[<b>RestrictEvents</b>](https://github.com/acidanthera/RestrictEvents/) - SoftwareUpdate Header file was used to learn and understand how to interact with ``_sysctl__children``. There may be snippets of code taken directly from RestrictEvents, such as Macros for the mapping of the oid list. If so, all appropriate credit is intended to be given to whoever worked on that!

<h6 align="center">A big thanks to all contributors and future contributors! ꩓</h6>
