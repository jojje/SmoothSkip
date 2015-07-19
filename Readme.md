# SmoothSkip

A filter plugin for AviSynth that finds the biggest frame difference(s) in cycles of frames and inserts new frames right before, frames picked from another clip.  The plugin's name hints at the original purpose, removing skips/jumps/"jerkiness" from clips that were badly recorded. See the [following][1] for a prime example.

*This filter was heavily inspired by Tritical's [TDecimate][3] filter which does the complete opposite (cyclically removing similar frames), and as such the user interface has been modeled to be familiar.*

## Usage
The filter signature is as follows
```
SmoothSkip( altClip, int "cycle", int "create", int "offset", int "dm", bool "debug" )

```
Options:

* `altclip`: The clip to pick frames from, to insert before the respective frames in a cycle having the highest frame difference to their respective preceding ones. Mandatory option.

* `cycle`: Number of consecutive frames forming a cycle between skips.  
Typically it would be the number of frames between skips. If the distance of the frame skipping varies, a larger cycle can be specified and number of frames to inject (*create*) in the cycle can also be raised so that an N in M cycle can be established.  
Default: `4`

* `create`: Number of frames to create in a cycle.  
This together with *cycle* forms the frequency of frame injection ("*create*" in "*cycle*"). For example if there is one skip seen in each cycle and a cycle is 4 frames, then one new frame will be injected every 4 frames. Consequently the resulting clip will have 5/4 as many frames and the frame rate increased by the same factor. If on the other hand, the cycle is less strict, such as 3 frames in every 10-12 frames, then *create* can be set to 3 with a *cycle* of the upper observed bound (12 frames), which will result in a 3 in 12 frame insertion ratio.  
Default: `1`

* `offset`: Frame offset used to pick an insertion frame from the alternate clip (*altclip*).  
When scripting AviSynth, users are prevented from accessing individual frames, as selecting frames is the purview of the application leveraging AviSynth. Filter plugins however have greater flexibility and can pick and choose which frame to use and this *offset* option gives you some flexibility in selecting replacement frames that do not necessarily reside at the same frame number as the primary clip's. Of course, you could achieve the effect of fixed offset like this by using *Trim* and *Loop*, so this is just a shorthand to save you from having to stitch and slice clips in the script.  
Default: `-1` (frame with number before current_frame)

* `dm`: Diff method. Valid options: 0 or 1.  
Scheme to use for determining how simiar / different a frame is to its preceding one. Supported schemes are:  
0: The built-in ydifference function is used to measure how similar frames are.  
1: Uses *FrameDiff* from the *TDecimate* filter plugin, which is more robust in certain scenarios. To use this option it is required that you've loaded Trical's filter into the script runtime, else no joy.  
Default: `0`

* `debug`: Display various internal metrics as an image overlay.  
Default: `false`


## Examples
Following are a couple of usage examples, from simple and trivial to more involved and interesting.

### Example 1
The following statement writes "bad frame" when the most dissimilar frame in a 5 frame cycle is encountered.
```
SmoothSkip( subtitle("bad frame"), cycle=5 )
```
### Example 2
A more interesting example is "repairing" bad frames. Since this plugin basically is just the "brains" deciding when to repair, the brawns need to come from somewhere else. Since we're dealing with motion, what better than [mvtools][2].
Let's assume we have the same problem as in example 1, with one bad frame every 5 frames
```
  avisource("myclip.avi")
  super = MSuper()
  bv = MAnalyse(super, overlap=4, isb = true, search=3)
  fv = MAnalyse(super, overlap=4, isb = false, search=3)
  inter = MFlowInter(super, bv, fv, time=50, ml=70)
  SmoothSkip(inter, cycle=5)

```
In this case we let mvtools "invent" (interpolate) a new frame between the "jumpy" ones and their preceding ones. The interpolation fraction was set to 50% (time=50) which means that a frame was created in an envisioned midpoint between the current skippy one and the previous. The smoothskip filter picks those intermediate frames when it sees bad (skippy) ones, resulting in much smoother motion.

### Example 3
The following example expands on example 2 by adding the TDecimate filter to the mix, wrapping it all up in an easy to use script function for the specific use case of smoothing out stuttering video. Video that stutters because there are duplicate frames, *as well* as skippy / jumpy ones.

```
# cycle is as described above and means the same thing to TDecimate and SmoothSkip.
# dupic means n.o. duplicate frames in cycle and corresponds to *cycleR* in TDecimate.
# skipic means skips in cycle and corresponds to *create* above.
# dm is the difference detection method, same as described above.
# debug has a new meaning, since all the filters have debug options. 
#   1 means show TDecimate debug overlay 
#   2 means the overlay from SmoothSkip
function SmoothMotion(clip last, int "cycle", int "dupic", int "skipic", int "dm", int "debug" ) {
  cycle  = default(cycle,  4)
  dupic  = default(dupic,  1)
  skipic = default(skipic, 1)
  dm     = default(dm,     1)
  debug  = default(debug,  0)
  TDecimate(cycle=cycle, cycleR=dupic, display=(debug == 1))
  super = MSuper()
  bv = MAnalyse(super, overlap=4, isb = true, search=3)
  fv = MAnalyse(super, overlap=4, isb = false, search=3)
  inter = MFlowInter(super, bv, fv, time=50, ml=70)
  SmoothSkip(inter, cycle=(cycle-dupic), create=skipic, offset=-1, dm=1, debug=(debug == 2))
}
avisource("myclip.avi")
SmoothMotion(cycle=4, debug=1)
```

Output:

![Processing illustration][img]

This is a sample clip that accentuates the problem this filter was designed to address.  
Upper left is the scene as it would have looked had it been recorded, encoded and/or transcoded correctly. Upper right is the pathetically bad result encoded by some *video monkey*, a botched job that makes the viewer's eyes bleed.
Lower right is the improvement we obtain by using TDecimate filter on bad clip. It's smoother than the original bad clip, but it's still skipping. And we also lost some frames and got a frame rate reduced clip. To the lower left the SmoothSkip filter has heuristically applied interpolated frames created by MVTools, thus restoring the motion close to the original scene (upper right). It has also restored the frame rate to that of the original clip.

There are some artifacts seen on the interpolated clip (lower left), and it's mvtool's doing. SmoothSkip just picks frames from clips generated by other filters, it doesn't create or process any itself. Admittedly, the used test-clip is rather nasty for motion interpolation, with a lot of geometry such algorithms have problems with. However using this trio of filters on *real* clips tend to yield much better result with less visible artifacts. For better results further processing, masking and tinkering is possible with the alt-clip, but the point of this illustration was to provide a sense of what sort of result can be expected from the filter when used with other good ones.

## License
Same base license as AviSynth; GNU GPL v2 or later.  

## Contribute
Fork it at [github], hack away and send a pull request.

[img]: https://griffeltavla.files.wordpress.com/2015/07/smoothskip-fast-white.gif
[1]: https://www.youtube.com/watch?v=j9cFHYHMQcY
[2]: http://avisynth.org.ru/mvtools/mvtools.html
[3]: http://avisynth.org.ru/docs/english/externalfilters/tivtc_tdecimate.htm
[github]: http://https://github.com/jojje/SmoothSkip
