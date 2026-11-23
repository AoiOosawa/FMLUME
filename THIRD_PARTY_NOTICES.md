# Third-party software

FMLUME is free software under the GNU Affero General Public License v3.0
(see `LICENSE`). Its source code is available from the place you got this
copy from. It is built with the following software:

| Software | Used for | Licence | Source |
|---|---|---|---|
| JUCE 9 | Plugin framework (VST3, Audio Unit, standalone app, user interface) | GNU AGPL v3.0 | https://github.com/juce-framework/JUCE |

JUCE includes further third-party code (among it the VST3 SDK by Steinberg
Media Technologies GmbH and the Audio Unit SDK by Apple Inc.); their licence
texts are in the JUCE source tree.

The synthesis engine, preset format, JSON reader, MIDI-file reader and test
harness are FMLUME's own code and do not use JUCE or any other library.

VST is a registered trademark of Steinberg Media Technologies GmbH.
Audio Units is a trademark of Apple Inc.

No fonts are included: the user interface and the logo use the fonts
installed on the system.

No third-party audio samples are included. All sounds, including the
factory presets, are produced by the built-in synthesiser.
