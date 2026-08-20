void test_dawson() {
  cout << "TMath::Dawson(1.0) = " << TMath::Dawson(1.0) << endl;
}
EOF
root -l -b -q test_dawson.C
