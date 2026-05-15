import tkinter as tk
from controller import CanalController


root = tk.Tk()
root.title("Scheduling Ships")

controller = CanalController(root)

root.mainloop()