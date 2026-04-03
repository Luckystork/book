import sys
try:
    from PIL import Image
except ImportError:
    print("Pillow not installed. Please run: pip install Pillow")
    sys.exit(1)

def remove_green(image_path, output_path):
    try:
        img = Image.open(image_path)
        img = img.convert("RGBA")
        datas = img.getdata()

        newData = []
        for item in datas:
            r, g, b, a = item
            # The background is #00FF00. We choke the edges by looking for dominant green.
            # If Green channel heavily outweighs Red and Blue, it's the spill.
            if g > 180 and r < 80 and b < 80:
                newData.append((255, 255, 255, 0))
            # Catching the softer anti-aliased edge spill
            elif g > r + 30 and g > b + 30 and g > 100:
                newData.append((255, 255, 255, 0))
            else:
                newData.append(item)

        img.putdata(newData)
        img.save(output_path, "PNG")
        print(f"Successfully processed image and saved to: {output_path}")
    except Exception as e:
        print(f"Error processing image: {e}")

input_img = "/Users/stansamsonau/.gemini/antigravity/brain/2fc9d469-56ac-44bc-b038-6541036ceb08/zeropoint_greenscreen_icon_1775060082496.png"
output_img = "/Users/stansamsonau/Documents/NiggaBook/build/zeropoint_logo_transparent.png"

print("Injecting Alpha Channel layer mapped to Chromium Green...")
remove_green(input_img, output_img)
