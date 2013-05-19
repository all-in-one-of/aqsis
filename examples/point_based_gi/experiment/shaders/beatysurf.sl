surface beatysurf(float Ks = 1;
		float Kd = 1;
		float Ki = 1;
		color Cemit = 0;
		color Cspec = 1;
		float microbufres = 20;
		float maxsolidangle = 0.03;
		float phong = -1;
		string diffusePointCloudName = "";
		string nonDiffusePointCloudName = "") {

	normal Nn = normalize(N);

	color indirect = Ki*indirect( diffusePointCloudName, nonDiffusePointCloudName, P, Nn, I,
				"maxsolidangle", maxsolidangle,
				"microbufres", microbufres,
				"phong", phong);


	color nondiff = Ki*indirect( "", nonDiffusePointCloudName, P, Nn, I,
				"maxsolidangle", maxsolidangle,
				"microbufres", microbufres,
				"phong", phong);	
	indirect = indirect + 6.28*nondiff;

	Oi = Os;
	color direct = 0;
	if (phong > 0) {
		direct = Cspec*Ks*phong(Nn,-I,phong)+Cs*Kd*diffuse(Nn);
	} else {
		direct = Cs*Kd*diffuse(Nn);
	}
	Ci=Oi*(direct+indirect);
}
