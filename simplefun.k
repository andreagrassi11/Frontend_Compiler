def g(x y)
    if x < y then
        var i = 0 in
            while i < 5 in
                i = i + 1 : x = x + 1
            end
        end
    else
       for k = 0 , k < 5 , 2 in
            y = y + 5
        end
    end;