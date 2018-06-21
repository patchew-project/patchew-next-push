---
layout: post
title:  "QEMU TCG Tests"
date:   2018-06-21 10:30:00:00 +0000
last_modified_at: 2018-06-21 10:30:00:00 +0000
author: Alex Bennée
categories: [testing, docker, compilation, tcg]
---

Ever since I started working on QEMU there was a small directory
called tests/tcg that was in a perpetually broken state. It contains
tests that exercise QEMU's ability to work across architectures using
the power of the Tiny Code Generator. However as these tests needed to
be compiled for the *guest* architectures and not the *host*
architecture - known as cross-compiling - most developers never ran
them. As the tests were hardly ever built inevitably a certain amount
of bit-rot set in.

# Cross Compilers

In the old days cross-compilation setups were almost all hand-crafted
affairs which involved building versions of binutils, gcc and a basic
libc. If you couldn't get someone to give you a pre-built tarball it
was something you laboured through once and hopefully never had to
touch again. There were even dedicated scripts like crosstool-ng which
attempted to make the process of patching and configuring your
toolchain easier.

While the distributions have improved their support for cross
compilers over the years there are still plenty of variations in how
they are deployed. It is hard for a project like QEMU which has to
build on a wide range of operating systems and architectures to
seamlessly use any given distributions compiler setup. However for
those with cross compilers to hand `configure` now accepts two
additional flags:

    --cross-cc-$(ARCH)
    --cross-cc-flags-$(ARCH)

With a compiler specified for each guest architecture you want to test
the build system can now build and run the tests. However for
developers that don't have cross compilers around they can now take
advantage of QEMU's docker images.

# Enter Docker Containers

If you work in IT you would be hard pressed not to have noticed the
hype around Docker and the concept of containerisation over the last
few years. Put simply containers allow you to define a known working
set of software that gets run in an isolated environment for a given
task. While this has many uses for QEMU it allows us to define build
environments that any developer can run without having to mess around
with their preferred host setup.

Over the last few years QEMU's build system has been expanding the
number of docker images it supports. Most of this has been in service
of our CI testing such as [Patchew](https://patchew.org/QEMU/) and
[Shippable](https://app.shippable.com/github/qemu/qemu/dashboard) but
any developer with a docker setup can run the exact same images. For
example if you want to check your patches won't break when compiled on
a 32 bit ARM system you can run:

    make docker-test-build@debian-armhf-cross J=n

instead of tracking down a piece of ARM hardware to actually build on.
Run `make docker` in your source tree to see the range of builds and
tests it can support.

# make check-tcg

With the latest work [merged into
master](https://git.qemu.org/?p=qemu.git;a=commit;h=TODO) we can now
take advantage of both hand configured and docker based cross
compilers to build test cases for TCG again. To run the TCG tests
after you have built QEMU:

    make check-tcg

and the build system will build and run all the tests it can for your
configured targets.

# Rules for tests/tcg

So now we have the infrastructure in place to add new tests what rules
need to be followed to add new tests? 

Well the first thing to note is currently all the tests are for
linux-user versions of QEMU. This means the tests are all currently
user-space tests that have access to the Linux syscall ABI.

Another thing to note is the tests are standalone from the rest of the
QEMU test infrastructure. To keep things simple they are compiled as
standalone "static" binaries. As the cross-compilation setup can be
quite rudimentary for some of the rarer architectures we only compile
against a standard libc. There is no support for linking to other
libraries like for example glib. Thread and maths support is part of
glibc so shouldn't be a problem.

Finally when writing new tests consider if it really is architecture
specific or can be added to `tests/tcg/multiarch`. The multiarch tests
are re-built for every supported architecture and should be the
default place for anything explicitly testing syscalls and other
common parts of the code base.

# What's Next

My hope with this work is we can start adding more tests to
systematically defend functionality in linux-user. In fact I hope the
first port of call to reproducing a crash would be writing a test case
that can be added to our growing library of tests.

Another thing that needs sorting out is getting toolchains for all of
the less common architectures. The current work relies heavily on the
excellent work of the Debian toolchain team in making multiarch
aware cross compilers available in their distribution. However QEMU
supports a lot more architectures than QEMU, some only as system
emulations. In principle supporting them is as easy as adding another
docker recipe but it might be these recipes end up having to compile
the compilers from source.

The `tests/tcg` directory still contains a number of source files we
don't build. Notably the cris, lm32, mips, openrisc and xtensa targets have
a set of tests that need a system emulator. Now we have the
infrastructure for compiling I hope we can get support for system
tests added fairly quickly. There will need to be some work to figure
out a nice common way to pass results back to the build-system. For
linux-user this is simple as all programs can simply return their exit
code however for system emulation this is a little more involved.

