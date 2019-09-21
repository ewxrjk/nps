nps
===

[![Build Status](https://travis-ci.org/ewxrjk/nps.svg?branch=master)](https://travis-ci.org/ewxrjk/nps)

This is an implementation of SUSv4 `ps`, and a compatible `top`, for
Linux.

It has the following features:

- Process properties: memory, IO, timing information, etc
- System properties: CPU usage, memory usage, load, etc
- Sort by combinations of properties
- Read defaults from a configuration file
- Flexible process selection (e.g. rss>1M, regexp match on command,
  etc)
- Flexible output formatting, including CSV output from ps
- Modify `top` settings on the the fly, without interrupting display
- On-screen help

⚠ I don't use this program any more, do not expect updates. ⚠

Installation
------------

    ./configure
    make
    sudo make install

See INSTALL for generic instructions.

Documentation
-------------

    nps --help
    nps --help-format
    man nps

    nps-top --help
    man nps-top

Within nps-top, press `h` to cycle through help.

Use
---

    nps
    nps -ef
    nps-top

If you like them more than the system versions:

    alias ps=nps
    alias top=nps-top

Bugs
----

Please report bugs via [Github](https://github.com/ewxrjk/nps).

Alternatively, email bug reports to Richard Kettlewell
<rjk@greenend.org.uk>.

Copyright
---------

Copyright © 2011-13 Richard Kettlewell

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA
