from scipy.io import wavfile
samplerate, rawdata = wavfile.read('./ring.wav')

def chunks(lst, n):
    """Yield successive n-sized chunks from lst."""
    for i in range(0, len(lst), n):
        yield lst[i:i + n]

file = open('loop.c', 'w+')
file.write('#include "main.h"\n')
min_data=min(rawdata)
max_data=max(rawdata)
max_abs=int(max(abs(min_data), abs(max_data)))
volume=512
idx=0
for data in chunks(rawdata, 8000):
	# shift data
	scale_factor=volume/(2*max_abs)
	shifted=list(map(lambda x: int((max_abs + x) * scale_factor), data))
	output=[]
	for i in shifted:
		output.append(str(i)+"u")
		output.append(str(volume - i)+"u")
	o=','.join(output)
	file.write('const uint16_t loop'+str(idx)+'['+str(len(output))+'] = {\n')
	file.write(o)
	file.write('\n};\n')
	idx+=1
