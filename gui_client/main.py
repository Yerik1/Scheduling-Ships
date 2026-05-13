import tkinter as tk
from controller import CanalController

if __name__ == "__main__":
    root = tk.Tk()
    root.geometry("1200x700")
    app = CanalController(root)
    root.mainloop()
