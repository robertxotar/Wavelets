function [Q_A,Q_S] = qmfilter(Wavelet)

Q_A = []; Q_S = []; 

        
% Biorthogonal Wavelets 
    
if Wavelet=='CDF_2.2'
        Q_A  = [-0.17677669529664   0.35355339059327   1.06066017177982  ...
                 0.35355339059327  -0.17677669529664                     ];
        Q_S  = [ 0.35355339059327   0.70710678118655   0.35355339059327  ]; 
elseif Wavelet=='CDF_2.4'
        Q_A  = [ 0.03314563036812  -0.06629126073624  -0.17677669529664  ...
                 0.41984465132951   0.99436891104358   0.41984465132951  ...
                -0.17677669529664  -0.06629126073624   0.03314563036812  ];
        Q_S  = [ 0.35355339059327   0.70710678118655   0.35355339059327  ];
                       
elseif Wavelet=='CDF_2.6'
        Q_A  = [-0.00690533966002   0.01381067932005   0.04695630968817  ...
                -0.10772329869639  -0.16987135563661   0.44746600996961  ...
                 0.96674755240348   0.44746600996961  -0.16987135563661  ...
                -0.10772329869639   0.04695630968817   0.01381067932005  ...
                -0.00690533966002                                        ];
        Q_S  = [ 0.35355339059327   0.70710678118655   0.35355339059327  ];                  
elseif Wavelet=='CDF_4.4'
	    Q_A  = [  .026748757411     -.016864118443     -.078223266529    ...
                  .266864118443      .602949018236      .266864118443    ...
                 -.078223266529     -.016864118443      .026748757411    ];% *sqrt(2);
	    Q_S  = [ -.045635881557     -.028771763114      .295635881557    ...      
                  .557543526229      .295635881557     -.028771763114    ...    
                 -.045635881557                                          ];% *sqrt(2);
        Q_A=Q_A*sqrt(2); Q_S=Q_S*sqrt(2);
elseif Wavelet==' VS_7.9'
    	Q_A  = [  .037828455506995  -.02384946501938   -.110624404418420  ... 
                  .377402855612650   .85269867900940    .377402855612650  ...
                 -.110624404418420  -.02384946501938    .037828455506995 ];
        Q_S = [  -.064538882628938  -.040689417609558   .418092273222210  ...   
                  .788485616405660   .418092273222210  -.040689417609558  ...  
                 -.064538882628938                                       ];     
end

% Copyright (c) 2014. S.E.Zarantonello