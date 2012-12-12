
PIFM - 

#http://www.icrobotics.co.uk/wiki/index.php/Turning_the_Raspberry_Pi_Into_an_FM_Transmitter
#http://hackaday.com/2012/12/10/transmit-fm-using-raspberry-pi-and-no-additional-hardware/
#http://www.reddit.com/r/raspberry_pi/comments/14k5o3/raspberry_pi_fm_transmitter_with_no_additional/  --havent read this yet...

-Added state to setup_fm, 0 off, 1 on
-Added shutdown function and call setup_fm(0) to shutdown carrier when exiting, suggested on Hackaday.
-Added variable frequency, (500/freq) * (16^3) - Found 16^3 in a comment. Source unknown. (might be better ways on reddit)
-Added variable bw, defaults to 25 if not given. I'm guessing this should only be lowered, suggested as usefull for amatuer frequencies
-Removed seeking to check size so stdin can be used. ./fm /dev/stdin 100.00 25                                                                                          


