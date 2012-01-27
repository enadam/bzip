#!/usr/bin/awk -f

function POW(x)
{
	return x;
	return x < 0 ? -(x * x) : (x * x);
}

BEGIN {
	epsilon = 0.08;

	nr = 0;
	usum = umin = umax = 0;
	sssm = smin = smax = 0;
}

{
	utime[nr] = $3;
	usum += $3;
	if ($3 < umin || umin == 0)
		umin = $3;
	else if ($3 > umax)
		umax = $3;

	stime[nr] = $6;
	ssum += $6;
	if ($6 < smin || smin == 0)
		smin = $6;
	else if ($6 > smax)
		smax = $6;

	nr++;
}

END {
	printf("%s:\n", toupper(FILENAME));

	uavg = usum / nr;
	uavgdif = umindif = umaxdif = 0;
	uales = uamor = 0;
	for (i = 0; i < nr; i++)
	{
		uavgdif += POW(utime[i] - uavg);
		umindif += POW(utime[i] - umin);
		umaxdif += POW(utime[i] - umax);
		uavgdifs[int((utime[i] - uavg) / epsilon)]++;

		if (utime[i] < uavg)
			uales++;
		else
			uamor++;
	}
	uavgdif /= nr;
	umindif /= nr;
	umaxdif /= nr;

	printf("user:\n");
	printf("\tsum: %g\n", usum);
	printf("\taverage: %g\n", uavg);
	printf("\tmin: %g\n", umin);
	printf("\tmax: %g\n", umax);
	printf("\tavgdif: %g\n", uavgdif);
	printf("\tmindif: %g\n", umindif);
	printf("\tmaxdif: %g\n", umaxdif);
	printf("\tales: %g\n", uales);
	printf("\tamor: %g\n", uamor);
	printf("\t-uavgdifs:");
	for (i = int((umin - uavg) / epsilon); i < 0; i++)
		printf(" %u", uavgdifs[i]);
	printf("\n");
	printf("\t+uavgdifs:");
	for (i = 0; i <= int((umax - uavg) / epsilon); i++)
		printf(" %u", uavgdifs[i]);
	printf("\n");

	printf("\n");
}
