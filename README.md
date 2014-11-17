ImplicitMapper
==============

**A graphical interface for managing supervised machine learning on [libmapper](http://libmapper.org) connections.**

ImplicitMapper is a reimplementation in Qt/C++ of the logic from the [implicitmap](https://github.com/malloch/implicitmap) MaxMSP external object and its MaxMSP graphical interface.

Background
----------

One of the design philosophies of the [libmapper](http://libmapper.org) project is avoid reinventing or reimplementing existing tools for mapping – instead we endeavour to make it easy to link disparate tools together with a low overhead for compatibility.  As an example, we linked several tools for using supervised machine learning for creating implicit mapping between gesture and sound synthesis, such as the [MnM](http://ftm.ircam.fr/index.php/MnM) tools from [IRCAM](http://www.ircam.fr/) ([Bevilacqua et al., NIME 2006](http://recherche.ircam.fr/equipes/temps-reel/articles/mnm.nime05.pdf)) with libmapper through a custom external object for [MaxMSP](https://cycling74.com/products/max/). This object – [implicitmap](https://github.com/malloch/implicitmap) – takes the place of the [regular MaxMSP binding objects for libmapper](https://github.com/malloch/mapper-max-pd), adding support for querying destination devices for their states (which we already supported through libmapper) and for dynamically adjusting the number of local inputs and outputs as necessary.

In addition, we created a new graphical interface in MaxMSP for supporting the needs of implicit mapping, with a live display of mapped input and output and curved lines to indicate sampling of the source and destination parameter spaces. Each time a snapshot is requested, libmapper handles retrieving the current destination state over the network, so parameters of the (e.g.) synthesizer can be adjusted using its own interface or manipulated using a 3rd souce of control data.  A “randomize” button was also added to allow quick exploration of the destination parameter space without leaving the implicit mapping GUI.

<div style="text-align:center">
<br/>
<img src ="https://josephmalloch.files.wordpress.com/2012/08/matmap.png" />
<br/>
<br/>
</div>


License
-------

This software is copyright those found in the AUTHORS file, and is licensed under the GNU Lesser Public General License version 2.1 or later. Please see COPYING for details.

In accordance with the LGPL, you are allowed to use it in commercial products provided it remains dynamically linked, such that this software always remains free to modify. If you'd like to use it in an outstanding context, please contact the AUTHORS to seek an agreement.

Dependencies
------------

* [libmapper](http://libmapper.org), LGPL
* [Qt](http://qt-project.org/) (v5.4 or later)
