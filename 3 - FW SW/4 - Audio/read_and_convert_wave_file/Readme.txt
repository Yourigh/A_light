DESCRIPTION:

"Snd Read Wave File in a Format.vi" opens an audio (.wav) file and converts any valid format of data to a specified audio format. The user can select whether the format is mono or stereo and whether it is 8 or 16 bits per sample.

This example program demonstrates how to open a audio file (.wav) in LabVIEW and get the data in a specified format regardless of the format found in the file. 

The VI "Snd Read Wave File in a Format.vi" is much like the "Snd Read Wave File" VI included in the sound palette in LabVIEW, but with the difference that in this VI the user can select the sound quality (mono/stereo) and the number of bits per sample (16 bits/8 bits). This example subVI also has 4 audio outputs but this time you can control which one will be active regardless of the type of data in the .wav file you select.

The VI has an input for a path to a .wav file. If this is unwired or if there is an empty path input, the VI opens a dialog window to select the file. There are also inputs to select the bits per sample and stereo or mono. The sampling rate is always kept for all audio files opened. "Snd Read Wave File in a Format.vi" also includes error inputs. If there is an incoming error, the VI does nothing but pass the error to the error output and return empty arrays of data.

The outputs of this VI are four terminals for audio, however, only the selected one is active (the others return an empty array). There are two outputs with a sound format cluster, one for the original format found in the file, and another one for the selected format (the one that the actual output is in). 

This VI is intended to be used where the processing of audio files is needed. It gives the user the ability to program for just one known format of audio data, regardless of the format found in the .wav file being opened. 

=====================================================================================


FILE INFORMATION:

There are two VI's included in this zip file:
1. "Snd Read Wave File in a Format.vi" - This VI can be used as a subVI that does the format conversion .
2. "Test for Snd Read Wave File in a Format.vi" - This VI is a simple example that demonstrates the use of "Snd Read Wave File in a Format.vi".

To view the functionality of the format conversion, run "Test for Snd Read Wave File in a Format.vi".

=====================================================================================


INPUTS:
	+File path: Input for a path to a .wav file. If  there is nothing connected to it the VI opens a file dialog.
	+Mono/Stereo input, selects the desired output audio queality.
	+16 bit/8 bit input, selects the bits per sample desired for the output
	+Error cluster in.

OUTPUTS:
	+Audio data. (only the output with the selected data format is active.)
	+Original audio format of the opened file.
	+Actual soud format of the data being sent.
	+Path of the opened file.
	+Error cluster out.

***Note: the sampling rate is not modified.