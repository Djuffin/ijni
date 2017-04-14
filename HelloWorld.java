import java.io.File;
public class HelloWorld {

	public static native int add(int a, int b);

    public static void main(String[] args) {
    	System.load(new File("./native_lib.so").getAbsolutePath());
    	int x = add(3, 4);
        System.out.println("The number is " + String.valueOf(x));
    	int y = add(3, 4);
        System.out.println("The number is " + String.valueOf(y));        
    }

}