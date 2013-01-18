surface indirect(float Ks = 1;
		float Kd = 1;
		float Ka = 1;
		float Ki = 1;
		float Ke = 1;
		color Cemit = 0;
		float microbufres = 20;
		float maxsolidangle = 0.03;
		float phong = -1;
		string diffusePointCloudName = "";
		string nonDiffusePointCloudName = "") {

	normal Nn = normalize(N);

	color indirect = indirect( diffusePointCloudName, nonDiffusePointCloudName, P, Nn, I,
				"maxsolidangle", maxsolidangle,
				"microbufres", microbufres,
				"phong", phong);

	Oi = Os;

	if (phong > 0) {
		Ci = Oi*(Ke*Cemit + Cs*(Ks*phong(Nn,I,phong) + Ka*ambient() + Ki*indirect));
	} else {
		Ci = Oi*(Ke*Cemit + Cs*(Kd*diffuse(Nn) + Ka*ambient() + Ki*indirect));
	}
	Ci = 3*indirect;
}
