FxPatch SDK (BETA)
============

This is a Software Development Kit for implementing your own effects for the [Polyend Endless](https://polyend.com/endless/) pedal, using hand-crafted code instead of the product's AI-agent capabilities.

Setup
----------

Except for your choice of standard tools (editor, IDE, and so on), you only need the [GNU Arm Embedded Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain) for your host. Refer to the toolchain site for installation instructions.

Develop
------------

The SDK contains an example implementation of a simple bitcrush effect in the [PatchImpl.cpp](source/PatchImpl.cpp) file.
You need to modify the implementation to create your effect.

When implementing your effect, keep in mind:
* Do not use heap/malloc/dynamic memory allocation. Keep data as members in your Patch implementation, or in the working buffer provided.
* If you hear artifacts or glitches on output, it's usually caused by one of the following:
  * Digital clipping of the signal: keep output values in the (-1.0f, 1.0f) range.
  * processAudio (or any other method of Patch) takes too much time and frames are dropped.

Build
------------
```
make TOOLCHAIN=/usr/bin/arm-none-eabi-
```
Or use your preferred tool to build the target binary from the C++ and C files in the `source` and `internal` directories, based on the process in the [Makefile](Makefile).

Deploy to device
----------

The process is the same as with agent-generated binaries. Connect the Endless to your host with a USB cable, then copy the resulting .bin file to the Endless drive.

For more details, refer to the Endless manual.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
