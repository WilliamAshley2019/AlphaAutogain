--------------------------------------------------------------------------------------------------
Copyright (c) 2026 William Ashley d/b/a William Ashley Music ( http://WilliamAshley.music )
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License  (v3) 

This program is distributed in the hope that it will be useful to other audio programmers and music makers in their own plugin designs.
There is no WARRANTY expressed or implied including for MERCHANTABILITY or FITNESS FOR ANY PURPOSE. 
See the GNU General Public License for more details.

Attributtion is requested where possible if you use or modify any of the source,
Notice of use is requested so I can familiarize myself with how the code has been adapted for personal interest.
contact@WilliamAshley.music   
-----------------------------------------------------------------------------------------------------
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![JUCE](https://img.shields.io/badge/Built%20with-JUCE%208.0.12-blue)](https://juce.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20-lightgrey)]()
[![Format](https://img.shields.io/badge/Format-VST3%20%7C%20-orange)]()
This is an autogain plugin that allows you to set limits and processing factors. It is fairly basic but fairly functional still testing. 

Future plans are to add in the a standard GUI that I have been working on that is still going through different iterations sort of recycle some gui chunks likely to add them into this for swaping out knobs and background
Not 100% sure what.   One knob is still a little glitchy I think due to a buffer size changing need to look into that.

License stuff as usually

Uses JUCE
This is intended as a VST3 aiming to be Stienberg compliant VST3 standard, the vst standard was developed by Steinberg and is one of their trademaarks, is steinberg if you didn't know. Hopefully it is in line havn't tested it enough to know, works for me.  I don't use their vst test software from the sdk only fl studio for plugin testing usually.

Made with Juce 8.0.12  using visual studio 2026 on windows 11 25h2

I will update this readme when I have sort of got to a level I intended to push this as a completed version, need to play with it a bit longer to figure out if there is any gaffs involved. Seems very functionable so far. 

This plugin allows you to set an operation range for the audio and the type of gain that is applied on the track it is placed on.

This was made with fl studio in mind based on a fl studio forum post about autogain so I'm like heck why not just make a plugin that does what the person wants to do and I added in some additional features etc.. 

and that is alpha autogain.

more to come when I get back to it, suppose to be playing wiht "Bass suite" plugins for bass mixing likely the next to be uploaded to github a series of plugins specific to help manage bass in a mix. 
