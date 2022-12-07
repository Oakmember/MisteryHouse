using UnityEngine;
using System.Collections;
using UnityEngine.InputSystem;

public class FreeCam : MonoBehaviour 
{
    public float speedNormal = 3f;
    public float speedFast = 45f;
    public float mouseSensX = 2f;
    public float mouseSensY = 2f;
    float rotY;
    float speed;
    
	void Start()
	{
        //Cursor.visible = false;
	}
   
	void Update()
	{
        //if (Mouse.current.leftButton.isPressed)
        //{
        //    //Cursor.visible = true;
        //}

        if (Mouse.current.rightButton.isPressed) 
        {
            float rotX = transform.localEulerAngles.y + (Mouse.current.delta.x.ReadValue() * mouseSensX);
            rotY += Mouse.current.delta.y.ReadValue() * mouseSensY;
            rotY = Mathf.Clamp(rotY, -80f, 80f);
            transform.localEulerAngles = new Vector3(-rotY, rotX, 0f);
            //Cursor.visible = false;
        }
        float forward = Mouse.current.delta.x.ReadValue(); //Input.GetAxis("Vertical");
        float side = Mouse.current.delta.y.ReadValue(); //Input.GetAxis("Horizontal");
        if (forward != 0f)  
        {
            if (Keyboard.current[Key.LeftShift].wasPressedThisFrame)
            {
                speed = speedFast;
            }
            else
            {
                speed = speedNormal;
            }
            Vector3 vect = new Vector3(0f, 0f, forward * speed * Time.deltaTime);
            transform.localPosition += transform.localRotation * vect;
        }
        if (side != 0f) 
        {
            if (Keyboard.current[Key.LeftShift].isPressed)
            {
                speed = speedFast;
            }
            else
            {
                speed = speedNormal;
            }
            Vector3 vect = new Vector3(side * speed * Time.deltaTime, 0f, 0f);
            transform.localPosition += transform.localRotation * vect;
        }
	}
}
