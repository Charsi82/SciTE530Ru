function fact(n)
	return (n>1) and n*fact(n-1) or 1
end

function sum2(m)
	local s = 0
	for i=1,m do
		s = s + math.floor( (math.cos(math.pi*(fact(m-1)+1)/m))^2 )
	end
	return s
end

function sum1(n, m)
	local s = 0
	for i = 1, m do
		s = s + math.floor( (n/sum2(i))^(1/n) )
	end
	return s
end

function prime(n)
	return 1 + sum1(n,2^n)
end

print( prime(4) )

