package myjni;

public class MyJNI {
	public native int exec(String filename);
	
	static {
		System.loadLibrary("myjni");
	}
}
