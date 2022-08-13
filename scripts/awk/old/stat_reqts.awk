BEGIN{
min = 0
max = 0

#usage: awk -f stat_warnsizekey.awk min=100 max=1000 tmp.log
#usage: awk -v min=100 -v max=1000 -f stat_warnsizekey.awk tmp.log
}
{
	reqTs=int(substr($14, 5, length($14)-5));

	if (reqTs >= min && reqTs <= max)
	{
		#print $1seprator$2seprator$3seprator$7seprator$8seprator$9seprator$13seprator$14seprator$15seprator$16seprator$17seprator$18seprator$19
		print $0
	}
}
END{

}